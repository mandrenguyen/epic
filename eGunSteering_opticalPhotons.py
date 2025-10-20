from DDSim.DD4hepSimulation import DD4hepSimulation
from g4units import mm, GeV
SIM = DD4hepSimulation()

SIM.gun.particle = "e-"
SIM.gun.momentumMin = 5.0*GeV
SIM.gun.momentumMax = 5.0*GeV
SIM.gun.multiplicity = 1

# target center (GLOBAL, mm)  -- central crystal in 5x5 cluster with normal incidence
# NOTE: geometry dump gave cm, converted ONCE to mm below.
TARGET_X = 0.0 * mm
TARGET_Y = -552.8 * mm    # -55.28 cm → -552.8 mm
TARGET_Z = -1850.1 * mm   # -185.01 cm → -1850.1 mm

# offset upstream from the face (mm): start 50 mm *toward the IP* (less negative z)
OFFSET = 50.0 * mm

# Gun start point: 50 mm in front of the face (less negative z)
seed_x = TARGET_X
seed_y = TARGET_Y
seed_z = TARGET_Z + OFFSET   # ≈ -18451.0 mm (50 mm upstream of face)

# Direction: from seed → target (shooting into the crystal)
vx = (TARGET_X - seed_x)
vy = (TARGET_Y - seed_y)
vz = (TARGET_Z - seed_z)
L = (vx*vx + vy*vy + vz*vz) ** 0.5
if L == 0:
    dx, dy, dz = (0.0, 0.0, -1.0)
else:
    dx, dy, dz = (vx / L, vy / L, vz / L)

SIM.gun.position  = (seed_x, seed_y, seed_z)
SIM.gun.direction = (dx, dy, dz)                    # ≈ (0, 0, -1)
SIM.gun.isotrop = False

# Explicitly set physics list and enable optical physics
SIM.physicsList = "FTFP_BERT"
SIM.enableOptical = True
