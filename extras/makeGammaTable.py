import math, argparse

def printGammaTable(gamma = 2.5, maxbright = 1.0, tablesize = 32):
    maxvalue = (tablesize - 1) * 256
    e = []
    for i in range(tablesize):
        e.append(0xff00 - int(math.pow(i * maxbright / (tablesize - 1) * maxvalue / (maxvalue - 1), gamma) * (0xff00) + .5))

    name = 'Gamma%i_%i' % (int(gamma), int(gamma * 10) % 10)
    if maxbright != 1.0:
        name += '_Brightness%i' % int(maxbright * 100)
    print('const uint16_t %s[%i] =' % (name, tablesize))
    print('{')
    s = '    '
    for i in range(tablesize):
        s += '0x%04x, ' % (e[i] & 0xffff)
        if (i & 7) == 7:
            print(s)
            s = '    '
    if s.strip():
        print(s)
    print('};')

def main():
    parser = argparse.ArgumentParser(description="SmoothLed gamma table calculator")
    parser.add_argument('-g', '--gamma', help='Gamma correction value (default 2.5)', type=float, default = 2.5)
    parser.add_argument('-b', '--maxbright', help='Maximum brightness value (0-1)', type=float, default = 1.0)
    parser.add_argument('-s', '--tablesize', help='Number of table entries (16-128)', type=int, default = 32)
    args = parser.parse_args()
    printGammaTable(args.gamma, args.maxbright, args.tablesize)

if __name__ == '__main__':
    main()