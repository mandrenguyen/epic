from DDSim.DD4hepSimulation import DD4hepSimulation

import logging
import sys
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
# Define a function to enable optical photons
def enableOpticalPhotons(kernel):
    # Import locally to ensure DDSim can see it
    from DDG4 import PhysicsList

    # Get physics list sequence
    seq = kernel.physicsList()

    # Add the Geant4 optical photon physics
    ph = PhysicsList(kernel, "Geant4OpticalPhotonPhysics/OpticalGammaPhys")
    ph.addParticleConstructor("G4OpticalPhoton")
    ph.VerboseLevel = 2
    seq.adopt(ph)

# Register the physics function
SIM.physics.setupUserPhysics(enableOpticalPhotons)

