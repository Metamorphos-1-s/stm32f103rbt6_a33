import struct, tempfile, unittest
from unittest import mock
from pathlib import Path
import swd_diagnostics as swd

class SwdDiagnosticsTests(unittest.TestCase):
    def control(self):
        words=[0]*swd.CONTROL_WORDS;words[0]=swd.MAGIC;words[1]=swd.VERSION;words[15]=swd.CONTROL_SIZE
        return struct.pack("<%dI"%swd.CONTROL_WORDS,*words)
    def snapshot(self):
        words=[0]*(swd.SNAPSHOT_PREFIX_WORDS+len(swd.COUNTER_NAMES));words[0]=swd.MAGIC;words[1]=swd.VERSION;words[24]=72000000;words[25]=1;words[26]=swd.TRACE_SIZE
        return struct.pack("<%dI"%len(words),*words)+bytes(swd.TRACE_SIZE*swd.TRACE_CAPACITY)
    def test_layout_and_decode(self):
        self.assertEqual(swd.TRACE_SIZE,28);self.assertEqual(swd.decode_control(self.control())["version"],3);self.assertEqual(swd.decode_snapshot(self.snapshot())["snapshot"]["cpu_clock_hz"],72000000)
    def test_truncation_corruption_and_version(self):
        with self.assertRaises(ValueError):swd.decode_control(self.control()[:-1])
        bad=bytearray(self.snapshot());bad[0]=0
        with self.assertRaises(ValueError):swd.decode_snapshot(bad)
        bad=bytearray(self.snapshot());struct.pack_into("<I",bad,4,99)
        with self.assertRaises(ValueError):swd.decode_snapshot(bad)
        bad=bytearray(self.snapshot());struct.pack_into("<I",bad,104,99)
        with self.assertRaises(ValueError):swd.decode_snapshot(bad)
    def test_delta_wrap(self):self.assertEqual(swd.delta32(0xFFFFFF00,0x100),0x200)
    def test_40hz_completion_accepts_automatic_restore_sequence(self):
        value={"applied_sequence":2,"trace_frozen":1,"rate_override_active":0,"status":2}
        self.assertTrue(swd.capture_complete(value,"40",1));self.assertFalse(swd.capture_complete(value,"10",1))
    def test_analysis_rate(self):
        data=bytearray(self.snapshot());base=swd.SNAPSHOT_PREFIX_WORDS*4
        values=[0]*len(swd.COUNTER_NAMES);values[0]=3;values[19]=0xFFF00000;values[20]=(0xFFF00000+14400000)&0xffffffff
        struct.pack_into("<%dI"%len(values),data,base,*values)
        got=swd.analyze(swd.decode_snapshot(data));self.assertAlmostEqual(got["ready_rate_hz"],10.0)
    def test_portable_outputs_and_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);control=root/"control.bin";snapshot=root/"snapshot.bin";out=root/"run"
            control.write_bytes(self.control());snapshot.write_bytes(self.snapshot());swd.decode_files(control,snapshot,out)
            for name in ("trace_decoded.json","trace_decoded.csv","counter_snapshot.json","rate_analysis.json"):
                data=(out/name).read_bytes();self.assertNotIn(b"\r\n",data);self.assertTrue(data.endswith(b"\n"))
            meta={"repository_commit":"a"*40,"firmware":{"elf":{"sha256":"B"*64}}};swd.manifest(out,meta)
            manifest=(out/"run_manifest_v2.json").read_bytes();self.assertNotIn(b"\r\n",manifest);self.assertTrue(manifest.endswith(b"\n"))
            parsed=__import__("json").loads(manifest);self.assertEqual(parsed["run_id"],"run")
    @mock.patch("swd_diagnostics.subprocess.run")
    def test_programmer_uses_hotplug(self, run):
        run.return_value.returncode=0;run.return_value.stdout="ok"
        result=swd.programmer_call("programmer","serial",1800,["-r32","0x20000000","1"])
        self.assertIn("mode=HotPlug",result["command"])
if __name__=="__main__":unittest.main()
