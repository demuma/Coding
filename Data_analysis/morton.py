def mortonEncode(col, row):
    morton = 0
    for i in range(16):
        morton |= (col & 1) << (2 * i)
        col >>= 1
        morton |= (row & 1) << (2 * i + 1)
        row >>= 1

    return morton

print(mortonEncode(1,1))



