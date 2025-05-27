#!/usr/bin/python3
# run driver on all traces and compare results against the reference
import subprocess as sp
import re

prog = "./sdriver.pl"
cmd = lambda t, s: " ".join([prog, "-t", t, "-s", s, "-a", "'-p'"])

def test(i):
    mine = sp.Popen(cmd("trace%s.txt" % i, "./tsh"), stdout=sp.PIPE, shell=True)
    ref = sp.Popen(cmd("trace%s.txt" % i, "./tshref"), stdout=sp.PIPE, shell=True)

    try:
        mine.wait(timeout=10)
    except sp.TimeoutExpired:
        print("Tsh timeout (failed to exit itself)")
        mine.terminate(), ref.terminate()
        return False

    mout, _ = mine.communicate() # stdout, stderr
    rout, _ = ref.communicate()

    # handle output of 'ps a'
    def cut(lines):
        ps = []
        for j in range(len(lines)):
            if not lines[j].startswith("  "):
                continue
            for k in range(j, len(lines)):
                if lines[k].find("mysplit") != -1:
                    ps.append(lines[k])
            lines = lines[:j]
            break
        return lines, ps

    mout, mps = cut(mout.decode().split('\n'))
    rout, rps = cut(rout.decode().split('\n'))

    if len(mout) != len(rout):
        print("tsh result:\n", "\n".join(mout))
        print("tshref result:\n", "\n".join(rout))
        print("Number of lines different from ref")
        return False

    for l1, l2 in zip(mout, rout):
        # ignore pid
        cl1 = re.sub(r"\([0-9]+\)", "", l1)
        cl2 = re.sub(r"\([0-9]+\)", "", l2)
        if cl1 != cl2:
            print("Line\n'%s'\nshould be\n'%s'" % (l1, l2))
            return False

    # 'ps a' results have some randomness,
    # so check them manually
    if mps or rps:
        print("mysplit entries in 'ps a':")
        print("  PID   TT  STAT      TIME COMMAND")
        print("tsh's:")
        print("\n".join(mps))
        print("tshref's:")
        print("\n".join(rps))

    print("Passed test %s" % i)
    return True

# run test on all 16 traces
for i in range(16):
    i = "0" + str(i+1)
    if not test(i[-2:]):
        break
