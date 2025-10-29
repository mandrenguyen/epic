#!/usr/bin/env python
"""
DD4hep simulation for EIC EPIC with optical photons enabled.
Generates both standard calorimeter hits and optical photon hits.
"""

from __future__ import absolute_import, unicode_literals
import logging
import sys

from DDSim.DD4hepSimulation import DD4hepSimulation
from DDG4 import PhysicsList


if __name__ == "__main__":

    logging.basicConfig(
        format='%(name)-16s %(levelname)s %(message)s',
        level=logging.INFO,
        stream=sys.stdout
    )
    logger = logging.getLogger('DDSim')

    # Create the simulation runner
    RUNNER = DD4hepSimulation()

    # --- Parse options and load detector XML ---
    RUNNER.parseOptions()
    RUNNER.physics.list = "FTFP_BERT_EMZ+optical"
    logger.info("Detector XML loaded successfully.")

    if 'opticalphotons' not in RUNNER.filter.filters:
        RUNNER.filter.filters['opticalphotons'] = {"name": "ParticleSelectFilter", "parameter": {"particle": "opticalphoton"}}
    
    RUNNER.filter.mapDetFilter['DRICH'] = 'opticalphotons'
    RUNNER.filter.mapDetFilter['RICHEndcapN'] = 'opticalphotons'
    RUNNER.filter.mapDetFilter['DIRC'] = 'opticalphotons'

    # Map both legacy and optical backward ECAL names to optical-photon handling
    for det in ('EcalEndcapN', 'EcalEndcapN_Optical'):
        RUNNER.filter.mapDetFilter[det] = 'opticalphotons'
        RUNNER.action.mapActions[det] = "Geant4OpticalTrackerAction"
    
    logger.info("Registered filters: %s", list(RUNNER.filter.filters.keys()))
    logger.info("Detector filter mapping:")
    for det, filt in RUNNER.filter.mapDetFilter.items():
        logger.info(f"  Detector: {det} -> Filter: {filt}")

    # --- Setup Cerenkov + optical physics ---
    def setupOptical(kernel):
        seq = kernel.physicsList()

        print("DEBUG: Adding Cerenkov physics.")
        cerenkov = PhysicsList(kernel, 'Geant4CerenkovPhysics/CerenkovPhys')
        cerenkov.MaxNumPhotonsPerStep = 10
        cerenkov.MaxBetaChangePerStep = 10.0
        cerenkov.TrackSecondariesFirst = False
        cerenkov.VerboseLevel = 0
        cerenkov.enableUI()
        seq.adopt(cerenkov)

        scint = PhysicsList(kernel, 'Geant4ScintillationPhysics/ScintPhys')
        scint.VerboseLevel = 0
        scint.enableUI()
        seq.adopt(scint)
        print("DEBUG: Scintillation physics added to physics list.")

        optical = PhysicsList(kernel, 'Geant4OpticalPhotonPhysics/OpticalGammaPhys')
        optical.VerboseLevel = 0
        optical.addParticleConstructor('G4OpticalPhoton')
        optical.enableUI()
        seq.adopt(optical)

    RUNNER.physics.setupUserPhysics(setupOptical)

    if hasattr(RUNNER.physics, "ESeverity"):
        RUNNER.physics.ESeverity = "IgnoreTheIssue"

    RUNNER.action.mapActions['DRICH'] = 'Geant4OpticalTrackerAction'
    RUNNER.action.mapActions['RICHEndcapN'] = 'Geant4OpticalTrackerAction'
    RUNNER.action.mapActions['DIRC'] = 'Geant4OpticalTrackerAction'

    if hasattr(RUNNER, "meta"):
        RUNNER.meta.addParametersToRunHeader = lambda *_: {}

    sys.exit(RUNNER.run())
