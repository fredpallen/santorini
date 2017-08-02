import itertools

BOARD_WIDTH = 5
CELLS = tuple([(x, y) for x in range(BOARD_WIDTH) for y in range(BOARD_WIDTH)])

def rot90(p):
    """Rotate a position tuple (x, y) 90 degrees."""
    x, y = p
    return (BOARD_WIDTH - 1 - y, x)

def reflect(p):
    """Reflect a position tuple (x, y)."""
    x, y = p
    return (BOARD_WIDTH - 1 - x, BOARD_WIDTH - 1 - y)

def rot90_all(p):
    """Rotate a tuple of pawn position tuples by 90 degrees."""
    (p11, p12), (p21, p22) = p
    return (tuple(sorted([rot90(p11), rot90(p12)])),
            tuple(sorted([rot90(p21), rot90(p22)])))

def reflect_all(p):
    """Reflect a tuple of pawn positions."""
    (p11, p12), (p21, p22) = p
    return (tuple(sorted([reflect(p11), reflect(p12)])),
            tuple(sorted([reflect(p21), reflect(p22)])))

generators = set()
positions = set()

for p in itertools.permutations(CELLS, r=4):
    position = (
            tuple(sorted([p[0], p[1]])),
            tuple(sorted([p[2], p[3]])))
#    print 'position =', position
#    print 'generator count =', len(generators)
    if position not in positions:
        print (
                '{p[0][0][0]} {p[0][0][1]} {p[0][1][0]} {p[0][1][1]} '
                '{p[1][0][0]} {p[1][0][1]} {p[1][1][0]} {p[1][1][1]}'
                .format(p=position))
        generators.add(position)
        for i in range(4):
#            print 'position =', position
            positions.add(position)
#            print 'reflected position =', reflect_all(position)
            positions.add(reflect_all(position))
            position = rot90_all(position)

#print 'Number of generators =', len(generators)
