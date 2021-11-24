import numpy as np
import cv2
import sys
import os

default_path = 'abberation_correction_data'
default_output = 'abberation_data.dat'

def cn(f):
    ss = f.readline().strip().strip('\"').strip()
    if ss:
        return ss
    return f.readline().strip().strip('\"')

def parse(lim, output=default_output, path=default_path):

    o = open(output, 'w')
    print(f'writing to {path}')

    pos = None
    for i in range(1, lim+1):
        index_file = os.path.join(path, str(i), 'index.txt')
        f = open(index_file, 'r')
        print(f'parsing {index_file}')

        maxi = 0
        phases = []
        data = []
        while line := cn(f):
            if not line:
                continue

            phase = float(line)
            name  = cn(f)
            imgname = os.path.join(path, str(i), name)
            print(f'reading {imgname}')
            img = cv2.imread(imgname)

            if pos is None:
                pos = (img.shape[0]//2, img.shape[1]//2)
            val = np.sqrt(np.linalg.norm(img[pos[0],pos[1]]/(3*255**2)))

            phases.append(phase)
            data.append(val)

            if data[maxi] < val:
                maxi = len(data) - 1
        print(f'{i} {maxi} ', end='', file=o)
        print(' '.join(map(str, data)), file=o)

        f.close()
    o.close()

if __name__ == '__main__':
    n       = len(sys.argv)
    inpath  = default_path
    outpath = default_output
    lim     = -1

    if n >= 2 and sys.argv[1] in ['-h', '--help']:
        print(f'parse.py [--lim lim] [--in input_path] [--out output_file]')
        sys.exit(0)

    i = 1
    while i < n:
        arg = sys.argv[i]
        if arg == '--lim':
            lim = int(argv[i+1])
            i += 1
        elif arg == '--in':
            inpath = argv[i+1]
            i += 1
        elif arg == '--out':
            outpath = argv[i+1]
            i += 1
        else:
            print(f'error: uknown option {arg}')
            print(f'parse.py [--lim lim] [--in input_path] [--out output_file]')
            sys.exit(1)

    if lim < 0:
        dirs = None
        for x in os.walk(inpath):
            dirs = x[1]
            break
        idirs = [int(x) for x in dirs if x.isdigit()]
        lim = max(idirs)
        print(f'found max index: {lim+1}')

    parse(lim, outpath, inpath)

