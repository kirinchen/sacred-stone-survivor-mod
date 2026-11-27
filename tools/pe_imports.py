"""PE 概況 + 匯入表。用法:uv run --with pefile python tools/pe_imports.py <exe> [dll過濾]"""
import sys, pefile
pe = pefile.PE(sys.argv[1])
want = sys.argv[2].lower() if len(sys.argv) > 2 else None
m = pe.FILE_HEADER.Machine
print(f"machine=0x{m:x} ({'x86' if m==0x14c else 'x64' if m==0x8664 else '?'})  imagebase=0x{pe.OPTIONAL_HEADER.ImageBase:x}  "
      f"entry=0x{pe.OPTIONAL_HEADER.AddressOfEntryPoint:x}  timestamp={pe.FILE_HEADER.TimeDateStamp}  "
      f"dllchars=0x{pe.OPTIONAL_HEADER.DllCharacteristics:x}")
for s in pe.sections:
    print(f"  section {s.Name.rstrip(b'\0').decode(errors='replace'):8} va=0x{s.VirtualAddress:x} vsize=0x{s.Misc_VirtualSize:x} raw=0x{s.SizeOfRawData:x}")
for d in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
    dll = d.dll.decode()
    if want and want not in dll.lower():
        continue
    names = [(imp.name.decode() if imp.name else f'#{imp.ordinal}') + f'@0x{imp.address:x}' for imp in d.imports]
    print(f"[{dll}] {len(names)}: " + ' '.join(names))
