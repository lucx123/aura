"""Bundle reproducible source and app image; exclude NVS backups and network captures."""
import hashlib
import json
import pathlib
import re
import zipfile


def main():
    root = pathlib.Path(__file__).resolve().parents[1]
    build = root / 'build'
    version = re.search(r'set\(PROJECT_VER\s+"([^"]+)"\)',
                        (root / 'CMakeLists.txt').read_text()).group(1)
    hashes = json.loads((build / 'sha256.json').read_text(encoding='utf-8-sig'))
    actual = hashlib.sha256((build / 'aura_watch.bin').read_bytes()).hexdigest().upper()
    assert actual == hashes['aura_watch.bin'], 'Build hash mismatch'
    files = [root / name for name in ('CMakeLists.txt', 'build-local.ps1',
        'dependencies.lock', 'sdkconfig.defaults', 'partitions.csv',
        'README.md', 'BASIC-1.0.md', 'ECLIPSE-0.1.md', 'AUDIT-2026-10-02.md', 'AUDIT-2026-10-03.md')]
    for directory in ('main', 'components', 'tools', 'tests'):
        files.extend(p for p in (root / directory).rglob('*')
                     if p.is_file() and '__pycache__' not in p.parts)
    reports = ('hardware-regression.json', 'features-check.json',
               'gesture-check.json', 'eclipse-navigation-check.json', 'engineer-check.json',
               'storage-check.json', 'storage-persistence-check.json', 'sleep-check.json',
               'scroll-benchmark.json', 'scroll-benchmark-33.json', 'scroll-benchmark-25.json',
               'menu-cache-check.json', 'idle-check.json', 'release-previews.json', 'host-verification.json')
    output = build / f'aura-watch-{version}.zip'
    with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as bundle:
        for path in sorted(files):
            bundle.write(path, 'source/' + path.relative_to(root).as_posix())
        bundle.write(build / 'aura_watch.bin', 'firmware/aura_watch.bin')
        bundle.writestr('firmware/sha256.json', json.dumps({'aura_watch.bin': actual}, indent=2)+'\n')
        for name in reports:
            if (build / name).is_file(): bundle.write(build / name, 'verification/' + name)
        if (build/'portal-expiry-check.json').is_file():
            bundle.write(build/'portal-expiry-check.json','verification/legacy/portal-expiry-check.json')
        for name in ('menu-top.png', 'menu-bottom.png', 'battery.png', 'cidr.png', 'eclipse.png',
                     'eclipse-bottom.png', 'home-hacker.png', 'rf.png', 'vlsm.png', 'engineer-keyboard.png', 'evidence.png'):
            if (build / name).is_file(): bundle.write(build / name, 'preview/' + name)
        bundle.writestr('LEEME.txt', f'''AURA Watch {version}
Placa: Waveshare ESP32-S3-Touch-AMOLED-2.06, inicializacion QSPI SH8601 local.
Imagen de aplicacion: firmware/aura_watch.bin
SHA-256: {actual}

Para actualizar esta unidad con sus particiones actuales, sin borrar preferencias:
python -m esptool --chip esp32s3 --port COM4 --baud 921600 write_flash 0x10000 firmware/aura_watch.bin
El puerto puede cambiar. Este paquete no cambia bootloader ni particiones.
Conserva tus respaldos originales por separado.

Fuente y documentacion: source/README.md y source/AUDIT-2026-10-03.md
Version de desarrollo. La fluidez tactil y autonomia requieren pruebas directas.
La tarjeta conectada es NTFS: Evidence usa un registro interno verificado.
FAT32/exFAT se probaron en imagenes sinteticas, no en una tarjeta fisica.
Los informes de verification excluyen nombres de redes; legacy conserva antecedentes.
''')
    digest = hashlib.sha256(output.read_bytes()).hexdigest().upper()
    output.with_suffix('.zip.sha256').write_text(f'{digest}  {output.name}\n')
    print(output)
    print(f'SHA-256: {digest}')


if __name__ == '__main__':
    main()
