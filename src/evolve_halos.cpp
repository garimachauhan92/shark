/*
 * SHArk_Evolve_Halos.cpp
 *
 *  Created on: 10Apr.,2017
 *      Author: clagos
 */

#include <cmath>
#include <memory>

#include "components.h"
#include "evolve_halos.h"
#include "logging.h"
#include "numerical_constants.h"

using namespace std;

namespace shark {

static
void evolve_system( shared_ptr<BasicPhysicalModel> physicalmodel, SubhaloPtr &subhalo, int snapshot, double z, double delta_t){

	// Solve ODEs for this system
	for(auto &galaxy: subhalo->galaxies) {
		physicalmodel->evolve_galaxy(*subhalo, *galaxy, z, delta_t);
		//Solve_Systems();
	}

}

void populate_halos(shared_ptr<BasicPhysicalModel> physicalmodel, HaloPtr halo, int snapshot, double z, double delta_t) {


	for(auto &subhalo: halo->all_subhalos()) {
		evolve_system(physicalmodel, subhalo, snapshot, z, delta_t);
	}
}

void transfer_galaxies_to_next_snapshot(const std::vector<HaloPtr> &halos, Cosmology cosmology){

	/**
	 * This function transfer galaxies of the subhalos of this snapshot into the subhalos of the next snapshot, and baryon components from subhalo to subhalo.
	 */

	// First make sure central subhalos at this snapshot have only one central galaxy.
	for(auto &halo: halos){
		for(SubhaloPtr &subhalo: halo->all_subhalos()) {

			// First check if this is the last snapshot this subhalo is identified. If so, galaxies have already been transferred in galaxy_mergers.cpp
			if(subhalo->last_snapshot_identified == subhalo->snapshot){
				continue;
			}

			vector<float> mbaryon;
			if(subhalo->subhalo_type == Subhalo::SATELLITE){
				for (auto &galaxy: subhalo->galaxies){
					if(galaxy->galaxy_type == Galaxy::CENTRAL){
						std::ostringstream os;
						os << "Satellite subhalo " << subhalo << " has at least 1 central galaxy";
						throw invalid_argument(os.str());
					}
				}
			}
			else{
				int i = 0;
				for (auto &galaxy: subhalo->galaxies){
					if(galaxy->galaxy_type == Galaxy::CENTRAL){
						i++;
						mbaryon.push_back(galaxy->baryon_mass());
					}
				}
				if(i==0){
					auto order_galaxies = subhalo->ordered_galaxies();
					if(order_galaxies.size() > 0){
						std::ostringstream os;
						os << "Central Subhalo " << subhalo << " has no central galaxy";
						throw invalid_data(os.str());
					}
				}
				if(i>1){
					std::ostringstream os;
					os << "Central Subhalo " << subhalo << " has " << i <<" central galaxies";
					os << "Baryon masses:" << mbaryon[0] << " " << mbaryon[1];
					throw invalid_argument(os.str());
				}
			}
		}
	}

	unsigned int subhalos_without_descendant = 0;
	for(auto &halo: halos){
		for(auto &subhalo: halo->all_subhalos()) {

			// First check if this is the last snapshot this subhalo is identified. If so, galaxies have already been transferred in galaxy_mergers.cpp
			if(subhalo->last_snapshot_identified == subhalo->snapshot){
				continue;
			}

			auto descendant_subhalo = subhalo->descendant;

			if (!descendant_subhalo) {
				subhalos_without_descendant++;
				continue;
			}

			// If the current subhalo is not a main progenitor, then make its central galaxy type 1. The other ones should already be type 1 or 2.
			if(!subhalo->main_progenitor or descendant_subhalo->subhalo_type == Subhalo::SATELLITE){
				auto central = subhalo->central_galaxy();
				if(central){
					central->galaxy_type = Galaxy::TYPE1;
				}
			}

			// Transfer galaxies.
			subhalo->transfer_galaxies_to(descendant_subhalo);

			// Transfer subhalo baryon components.
			descendant_subhalo->cold_halo_gas = subhalo->cold_halo_gas;
			descendant_subhalo->hot_halo_gas = subhalo->hot_halo_gas;
			descendant_subhalo->ejected_galaxy_gas = subhalo->ejected_galaxy_gas;
			descendant_subhalo->cooling_subhalo_tracking = subhalo->cooling_subhalo_tracking;
		}
	}

	// Make sure there is only one central galaxy per halo/central in the descendant subhalos.
	for(auto &halo: halos){
		for(SubhaloPtr &subhalo: halo->all_subhalos()) {
			auto descendant_subhalo = subhalo->descendant;
			if(!descendant_subhalo){
				continue;
			}
			if(descendant_subhalo->subhalo_type == Subhalo::SATELLITE){
				for (auto &galaxy: descendant_subhalo->galaxies){
					if(galaxy->galaxy_type == Galaxy::CENTRAL){
						std::ostringstream os;
						os << "Satellite subhalo " << descendant_subhalo << " has at least 1 central galaxy";
						throw invalid_argument(os.str());
					}
				}
			}
			else{ // subhalo is central.
				int i = 0;
				for (auto &galaxy: descendant_subhalo->galaxies){
					if(galaxy->galaxy_type == Galaxy::CENTRAL){
						i++;
					}
				}
				if(i==0){
					auto order_galaxies = descendant_subhalo->ordered_galaxies();
					if(order_galaxies.size() > 0){
						std::ostringstream os;
						os << "Central Subhalo " << descendant_subhalo << " has no central galaxy";
						throw invalid_data(os.str());
					}
				}
				if(i>1){
					std::ostringstream os;
					os << "Central Subhalo " << descendant_subhalo << " has " << i <<" central galaxies";
					throw invalid_argument(os.str());
				}
			}
		}
	}

	// Now calculated accreted hot mass by assuming mass conservation and assuming the universal baryon fraction.

	/*for(auto &halo: halos){

		auto desc_halo = halo->descendant;

		double total_baryon_mass = 0.0;
		for (auto &subhalo: halo->all_subhalos()){
			total_baryon_mass += subhalo->total_baryon_mass();
		}

		double mbar_desc = desc_halo->Mvir * cosmology.universal_baryon_fraction();

		double mass_acc = mbar_desc - total_baryon_mass;
		if(mass_acc < 0){
			mass_acc = 0;
		}

		desc_halo->central_subhalo->accreted_mass = mass_acc;

	}*/

	if (subhalos_without_descendant) {
		LOG(warning) << "Found " << subhalos_without_descendant << " subhalos without descendant while transferring galaxies";
	}

}

void track_total_baryons(StarFormation &starformation, const std::vector<HaloPtr> &halos, TotalBaryon &AllBaryons, double redshift, int snapshot){


	BaryonBase mcold_total;
	BaryonBase mhothalo_total;
	BaryonBase mcoldhalo_total;
	BaryonBase mejectedhalo_total;
	BaryonBase mstars_total;
	BaryonBase MBH_total;
	BaryonBase mHI_total;
	BaryonBase mH2_total;
	BaryonBase mDM_total;

	double SFR_total = 0;

	double total_baryons = 0;

	// Loop over all halos and subhalos to write galaxy properties
	for (auto &halo: halos){

		// accummulate dark matter mass
		mDM_total.mass += halo->Mvir;

		for (auto &subhalo: halo->all_subhalos()){

			// Accummulate subhalo baryons
			mhothalo_total.mass += subhalo->hot_halo_gas.mass;
			mhothalo_total.mass_metals += subhalo->hot_halo_gas.mass_metals;

			mcoldhalo_total.mass += subhalo->cold_halo_gas.mass;
			mcoldhalo_total.mass_metals += subhalo->cold_halo_gas.mass_metals;

			mejectedhalo_total.mass += subhalo->ejected_galaxy_gas.mass;
			mejectedhalo_total.mass_metals += subhalo->ejected_galaxy_gas.mass_metals;

			for (auto &galaxy: subhalo->galaxies){

				//Accummulate galaxy baryons

				double m_mol;
				double m_atom;
				double m_mol_b;
				double m_atom_b;
				starformation.get_molecular_gas(galaxy, redshift, &m_mol, &m_atom, &m_mol_b, &m_atom_b);

				mHI_total.mass += m_atom+m_atom_b;
				mH2_total.mass += m_mol+m_mol_b;

				mcold_total.mass += galaxy->disk_gas.mass + galaxy->bulge_gas.mass;
				mcold_total.mass_metals += galaxy->disk_gas.mass_metals + galaxy->bulge_gas.mass_metals;

				mstars_total.mass += galaxy->disk_stars.mass + galaxy->bulge_stars.mass;
				mstars_total.mass_metals += galaxy->disk_stars.mass_metals + galaxy->bulge_stars.mass_metals;

				SFR_total += galaxy->sfr_disk + galaxy->sfr_bulge;

				MBH_total.mass += galaxy->smbh.mass;
			}
		}
	}

	total_baryons = mstars_total.mass + mcold_total.mass + MBH_total.mass + mhothalo_total.mass + mcoldhalo_total.mass + mejectedhalo_total.mass;

	AllBaryons.mstars.push_back(mstars_total);
	AllBaryons.mcold.push_back(mcold_total);
	AllBaryons.mHI.push_back(mHI_total);
	AllBaryons.mH2.push_back(mH2_total);
	AllBaryons.mBH.push_back(MBH_total);
	AllBaryons.SFR.push_back(SFR_total);

	AllBaryons.mhot_halo.push_back(mhothalo_total);
	AllBaryons.mcold_halo.push_back(mcoldhalo_total);
	AllBaryons.mejected_halo.push_back(mejectedhalo_total);

	AllBaryons.mDM.push_back(mDM_total);

	// Test for mass conservation.

	double frac = std::abs(total_baryons-AllBaryons.baryon_total_created[snapshot])/std::max(AllBaryons.baryon_total_created[snapshot],total_baryons);

	if(frac > constants::EPS3){
		/*std::ostringstream os;
		os << "Accummulated baryon mass, " << total_baryons << " differs by more than " << constants::EPS3 << " than the baryon mass created by this snapshot, "<< AllBaryons.baryon_total_created[snapshot];
		throw invalid_data(os.str());*/
		LOG(warning) << "Accummulated baryon mass differs by " << frac << " with the baryon mass created by this snapshot. The ratio between the two is: "<< total_baryons/AllBaryons.baryon_total_created[snapshot+1];
	}

}

}
