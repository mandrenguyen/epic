#!/usr/bin/env python
"""
DD4hep simulation for EIC EPIC with optical photons enabled.
Generates both standard calorimeter hits and optical photon hits.
"""

from __future__ import absolute_import, unicode_literals
import logging
import sys

from DDSim.DD4hepSimulation import DD4hepSimulation
import DDG4
from DDG4 import Filter, PhysicsList


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
    # Clean up possible leftover steering content before run initialization
    for attr in ["SteeringFileContent", "steeringFileContent", "SteeringFile"]:
        if hasattr(RUNNER, attr):
            delattr(RUNNER, attr)
    RUNNER.physics.list = "FTFP_BERT_EMZ+optical"
    logger.info("Detector XML loaded successfully.")



    # --- Define filters (after parseOptions) ---
    #RUNNER.filter.filters['edep0'] = {"name": "EDepFilter","parameter": {}}




    # Only create the filter if not already present
    # Removed creation of 'edep0' filter as per instructions

    if 'opticalphotons' not in RUNNER.filter.filters:
        RUNNER.filter.filters['opticalphotons'] = {"name": "ParticleSelectFilter", "parameter": {"particle": "opticalphoton"}}
    
    RUNNER.filter.mapDetFilter['DRICH'] = 'opticalphotons'
    RUNNER.filter.mapDetFilter['RICHEndcapN'] = 'opticalphotons'
    RUNNER.filter.mapDetFilter['DIRC'] = 'opticalphotons'

    # --- Map filters and actions for ECAL ---
    # Removed standard calorimeter hits mapping since non-optical calorimeter is disabled
    #RUNNER.filter.mapDetFilter['EcalEndcapNHits'] = 'edep0'
    #RUNNER.action.mapActions['EcalEndcapNHits'] = "Geant4ScintillatorCalorimeterAction"
    
    # Optical photons in dedicated SD
    RUNNER.filter.mapDetFilter['EcalEndcapNOpticalHits'] = 'opticalphotons'
    RUNNER.action.mapActions['EcalEndcapNOpticalHits'] = "Geant4OpticalTrackerAction"
    
    # Optional debug: check filter mapping
    logger.info("Registered filters: %s", list(RUNNER.filter.filters.keys()))
    logger.info("Detector filter mapping:")
    for det, filt in RUNNER.filter.mapDetFilter.items():
        logger.info(f"  Detector: {det} -> Filter: {filt}")

    # --- Setup Cerenkov + optical physics ---
    def setupOptical(kernel):
        seq = kernel.physicsList()

        # Cerenkov physics
        cerenkov = PhysicsList(kernel, 'Geant4CerenkovPhysics/CerenkovPhys')
        cerenkov.MaxNumPhotonsPerStep = 10
        cerenkov.MaxBetaChangePerStep = 10.0
        cerenkov.TrackSecondariesFirst = False
        cerenkov.VerboseLevel = 0
        cerenkov.enableUI()
        seq.adopt(cerenkov)

        # Optical photons
        optical = PhysicsList(kernel, 'Geant4OpticalPhotonPhysics/OpticalGammaPhys')
        optical.VerboseLevel = 0
        optical.addParticleConstructor('G4OpticalPhoton')
        optical.enableUI()
        seq.adopt(optical)

    RUNNER.physics.setupUserPhysics(setupOptical)

    # Disable warnings for off-shell resonances
    if hasattr(RUNNER.physics, "ESeverity"):
        RUNNER.physics.ESeverity = "IgnoreTheIssue"

    # --- Map actions ---
    RUNNER.action.mapActions['DRICH'] = 'Geant4OpticalTrackerAction'
    RUNNER.action.mapActions['RICHEndcapN'] = 'Geant4OpticalTrackerAction'
    RUNNER.action.mapActions['DIRC'] = 'Geant4OpticalTrackerAction'


    # --- Optical photon efficiency stacking for calorimeter ---
    # Removed to avoid double-counting efficiency since bordersurfaces handle EFFICIENCY
    #RUNNER.action.stack = [
    #    {
    #        "name": "OpticalPhotonEfficiencyStackingAction",
    #        "parameter": {
    #            "LambdaMin": "180*nm",
    #            "LambdaMax": "678*nm",
    #            "LogicalVolume": "EcalEndcapN",
    #            "Efficiency": [1.0]*101  # all wavelengths fully efficient
    #        }
    #    }
    #]


    # --- Disable DD4hep metadata RunHeader injection (avoids std::map conversion crash) ---
    if hasattr(RUNNER, "meta"):
        RUNNER.meta.addParametersToRunHeader = lambda *_: {}

    # --- Run simulation ---
    sys.exit(RUNNER.run())
