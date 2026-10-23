from pathlib import Path
import json,struct
r=Path.cwd();t=r/'.clanker/tasks/native-island-crossing-repair-20261023';base=r/'var/test-saves/native-island-crossing-repair-20261023';legacy=(base/'original.stl').read_bytes();partial=(base/'partial.stlc').read_bytes();assert legacy[:4]==b'STL9';assert partial[:4]==b'STLC' and struct.unpack('<I',partial[4:8])[0]==1;(t/'save-format-verification.json').write_text(json.dumps({'original_magic':'STL9','partial_magic':'STLC','partial_binary_version':1,'passed':True},indent=2)+'\n');print('SAVED_FORMATS_VERIFIED')
