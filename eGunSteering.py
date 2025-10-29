from DDSim.DD4hepSimulation import DD4hepSimulation
from g4units import mm, GeV, MeV, deg
SIM = DD4hepSimulation()

SIM.gun.particle = "e-"
SIM.gun.distribution = "eta"
SIM.gun.momentumMin = 4*GeV
SIM.gun.momentumMax = 5*GeV


'''
SIM.gun.thetaMin = 177*deg
SIM.gun.thetaMax = 135*deg
'''

SIM.gun.phiMax = 10*deg
SIM.gun.phiMin = 11*deg
SIM.gun.thetaMax = 171.*deg
SIM.gun.thetaMin = 169.*deg

#SIM.gun.distribution = None
#SIM.gun.direction = (0.219, 0.039, -0.975)


#SIM.gun.momentumMin = 4.999*GeV
#SIM.gun.momentumMax = 5.001*GeV
