"""DM-FC01 CAN IMU v1. Standard library only; SI units, device boot microseconds."""
import binascii
import collections
import math
import struct

DATA = 0x680
STATUS = 0x6A0
SYNC_REQUEST = 0x6C0
SYNC_REPLY = 0x6B0
EFF = 0x80000000
RTR = 0x40000000
ERR = 0x20000000
FRAME = struct.Struct('=IB3x8s')
SAMPLE = struct.Struct('<Q6fIHHhBBHH')


def socket_filters():
    # CAN_ERR_FLAG in a Linux filter mask selects the error-frame receive list.
    # Match standard data frames only; leave the three fragment-index bits free.
    mask = EFF | RTR | 0x7F8
    return b''.join(struct.pack('=II', base, mask) for base in (DATA, STATUS, SYNC_REPLY))


def crc(base, seq, payload):
    return binascii.crc_hqx(struct.pack('<IH', base, seq) + payload, 0xFFFF)


def fragments(base, seq, payload):
    payload = payload + struct.pack('<H', crc(base, seq, payload))
    if len(payload) % 6:
        raise ValueError('payload including CRC must be a multiple of six')
    return [(base + i // 6, struct.pack('<H', seq) + payload[i:i+6])
            for i in range(0, len(payload), 6)]


class Decoder:
    """Bounded, expiring reassembly. Only increasing samples within a boot are delivered."""
    def __init__(self):
        self.pending = {}
        self.counts = collections.Counter()
        self.boot = None
        self.last_time = None
        self.last_seq = None
        self.retired = collections.deque(maxlen=32)

    def feed(self, can_id, data, now):
        if can_id & (EFF | RTR | ERR) or can_id > 0x7FF or len(data) != 8:
            return None
        ident = can_id & 0x1FFFFFFF
        base, index = ident & ~7, ident & 7
        size = {DATA: 8, STATUS: 8, SYNC_REPLY: 4}.get(base)
        if size is None or index >= size:
            return None
        for key in list(self.pending):
            if now - self.pending[key][0] > .020:
                del self.pending[key]
                self.counts['fragment_timeout'] += 1
        seq, = struct.unpack_from('<H', data)
        key = base, seq
        if key not in self.pending:
            if len(self.pending) >= 32:
                del self.pending[next(iter(self.pending))]
                self.counts['reassembly_overflow'] += 1
            self.pending[key] = (now, {})
        first, pieces = self.pending[key]
        if index in pieces:
            self.counts['duplicate_fragment'] += 1
            if pieces[index] != data[2:]:
                del self.pending[key]
                self.counts['conflicting_fragment'] += 1
            return None
        pieces[index] = data[2:]
        if len(pieces) != size:
            return None
        del self.pending[key]
        payload = b''.join(pieces[i] for i in range(size))
        if crc(base, seq, payload[:-2]) != struct.unpack_from('<H', payload, len(payload)-2)[0]:
            self.counts['crc_error'] += 1
            return None
        self.counts['packets'] += 1
        if base == DATA:
            t, *fields = SAMPLE.unpack(payload)
            vector = fields[:6]
            boot, dt, flags, temperature, source, version, epoch, _ = fields[6:]
            if version != 1 or source not in (0, 1) or dt != 5000 or not all(map(math.isfinite, vector)):
                self.counts['invalid_payload'] += 1
                return None
            if boot != self.boot:
                if boot in self.retired:
                    self.counts['old_session'] += 1
                    return None
                if self.boot is not None:
                    self.retired.append(self.boot)
                    self.counts['restarts'] += 1
                self.boot, self.last_time, self.last_seq = boot, None, None
                self.pending.clear()
            if self.last_time is not None:
                if t <= self.last_time:
                    self.counts['out_of_order'] += 1
                    return None
                self.counts['missing_samples'] += max(0, round((t-self.last_time)/dt)-1)
            self.last_time, self.last_seq = t, seq
            self.counts['samples'] += 1
            return dict(kind='sample', seq=seq, device_us=t, accel=vector[:3], gyro=vector[3:],
                        boot=boot, window_us=dt, flags=flags, temperature_c=None if temperature == -32768 else temperature/100,
                        source=source, version=version, epoch=epoch, receive_mono=now, first_fragment_mono=first)
        if base == SYNC_REPLY:
            t2, t3, boot, version, _ = struct.unpack('<QQIHH', payload)
            return dict(kind='sync', seq=seq, t2=t2, t3=t3, boot=boot, version=version, receive_mono=now)
        t, boot, tx_drop, incomplete, can_errors, orb_lost, gap0, gap1, temp0, temp1, source, version, epoch, heat_fault, calibration, _ = struct.unpack('<Q7I2h2BH2BH', payload)
        return dict(kind='status', device_us=t, boot=boot, tx_drop=tx_drop, incomplete=incomplete,
                    can_errors=can_errors, orb_lost=orb_lost, source_gaps=[gap0,gap1],
                    temperatures=[temp0/100,temp1/100], source=source, version=version,epoch=epoch,
                    heater_fault=heat_fault,calibration=calibration,receive_mono=now)


class ClockMap:
    """Software four-event mapping. Residuals do not establish absolute accuracy."""
    def __init__(self):
        self.boot = None
        self.points = collections.deque(maxlen=300)
        self.a, self.b = 1., None
        self.last = 0.
        self.residual = None
        self.min_rtt = None

    def update(self, t1, reply):
        t2, t3, t4 = reply['t2']*1e-6, reply['t3']*1e-6, reply['receive_mono']
        rtt = (t4-t1)-(t3-t2)
        if t3 < t2 or not 0 <= rtt <= .010 or reply['version'] != 1:
            return False
        if self.boot != reply['boot']:
            self.__init__()
            self.boot = reply['boot']
        self.points.append(((t2+t3)/2, (t1+t4)/2, rtt))
        best = sorted(self.points,key=lambda p:p[2])[:max(3,len(self.points)//3)]
        self.min_rtt = best[0][2]
        mx = sum(p[0] for p in best)/len(best)
        my = sum(p[1] for p in best)/len(best)
        xx = sum((p[0]-mx)**2 for p in best)
        a = sum((p[0]-mx)*(p[1]-my) for p in best)/xx if xx > 1 else 1.
        if not .999 <= a <= 1.001:
            return False
        self.a, self.b, self.last = a, my-a*mx, t4
        self.residual = max(abs(y-(a*x+self.b)) for x,y,_ in best)
        return True

    def map(self, device_us, boot, now):
        if boot != self.boot or self.b is None or now-self.last > 1.0 or len(self.points)<5:
            return None
        return self.a*device_us*1e-6+self.b
