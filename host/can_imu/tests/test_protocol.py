import random
import struct
import unittest
from protocol import *


def sample(seq=1,t=5000,boot=1):
    payload=SAMPLE.pack(t,1,2,3,.1,.2,.3,boot,5000,1,4800,0,1,1,0)
    return fragments(DATA,seq,payload[:-2])


class ProtocolTests(unittest.TestCase):
    def feed(self,d,frames,start=0):
        result=None
        for i,(ident,data) in enumerate(frames):
            event=d.feed(ident,data,start+i*.0001)
            if event: result=event
        return result

    def test_crc_reference(self):
        self.assertEqual(binascii.crc_hqx(b'123456789',0xffff),0x29b1)

    def test_order_duplicates_and_roundtrip(self):
        d=Decoder(); frames=sample(); random.Random(1).shuffle(frames)
        frames.insert(1,frames[0]); r=self.feed(d,frames)
        self.assertEqual(r['device_us'],5000)
        self.assertEqual(r['accel'],[1,2,3]); self.assertEqual(d.counts['duplicate_fragment'],1)

    def test_loss_crc_and_timeout(self):
        d=Decoder(); self.assertIsNone(self.feed(d,sample()[:-1]))
        self.assertIsNotNone(self.feed(d,sample(2,10000),.1))
        self.assertEqual(d.counts['fragment_timeout'],1)
        frames=sample(3,15000); i,b=frames[3]; frames[3]=(i,b[:-1]+bytes([b[-1]^1]))
        self.assertIsNone(self.feed(d,frames,.2)); self.assertEqual(d.counts['crc_error'],1)

    def test_wrap_reboot_and_old_replay(self):
        d=Decoder(); self.feed(d,sample(65535,5000)); self.feed(d,sample(0,10000),.01)
        self.assertEqual(d.counts['missing_samples'],0)
        self.feed(d,sample(1,5000,2),.02)
        self.assertIsNone(self.feed(d,sample(1,15000,1),.03))
        self.assertEqual(d.counts['restarts'],1)

    def test_missing_invalid_and_bounded_memory(self):
        d=Decoder(); self.feed(d,sample());self.feed(d,sample(4,20000),.001)
        self.assertEqual(d.counts['missing_samples'],2)
        for seq in range(100): self.feed(d,sample(seq+10,50000)[:1],.002)
        self.assertLessEqual(len(d.pending),32)
        self.assertIsNone(d.feed(EFF|DATA,b'12345678',0))

    def test_motor_id_isolation_and_frame_type(self):
        ids = [DATA+i for i in range(8)] + [STATUS+i for i in range(8)] + [SYNC_REPLY+i for i in range(4)] + [SYNC_REQUEST]
        self.assertEqual(len(ids), len(set(ids)))
        for ident in ids:
            self.assertTrue(0x100 <= ident < 0x7ff)
            self.assertNotIn(ident & 0xff, (1,2,3,4))
        for flag in (EFF,RTR,ERR):
            self.assertIsNone(self.feed(Decoder(),[(ident|flag,data) for ident,data in sample()]))
        for ident in (1,2,3,4,0x7ff):
            self.assertIsNone(Decoder().feed(ident,b'12345678',0))

    def test_clock_drift_and_expiry(self):
        clock=ClockMap()
        for i in range(200):
            device=100+i*.1;host=1.00002*device+500
            r=dict(t2=int(device*1e6),t3=int((device+.0001)*1e6),receive_mono=host+.0005+.0001,boot=1,version=1)
            self.assertTrue(clock.update(host-.0005,r))
        self.assertAlmostEqual(clock.a,1.00002,places=6)
        self.assertIsNotNone(clock.map(110000000,1,clock.last))
        self.assertIsNone(clock.map(110000000,1,clock.last+2))

if __name__=='__main__': unittest.main()
