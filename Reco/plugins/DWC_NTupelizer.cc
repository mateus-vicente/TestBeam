/* 
 * 
 */

/**
	@Author: Thorben Quast <tquast>
		21 July 2017
		thorben.quast@cern.ch / thorben.quast@rwth-aachen.de
*/


// system include files
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <math.h>
// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "HGCal/DataFormats/interface/HGCalTBWireChamberData.h"
#include "HGCal/DataFormats/interface/HGCalTBRunData.h"	//for the runData type definition
#include "HGCal/Reco/interface/Tracks.h"
#include "HGCal/Reco/interface/Sensors.h"

#include "TTree.h"
#include "TFile.h"


class DWC_NTupelizer : public edm::one::EDAnalyzer<edm::one::SharedResources> {
	public:
		explicit DWC_NTupelizer(const edm::ParameterSet&);
		~DWC_NTupelizer();
		static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

	private:
		virtual void beginJob() override;
		void analyze(const edm::Event& , const edm::EventSetup&) override;
		virtual void endJob() override;

		// ----------member data ---------------------------

		edm::EDGetTokenT<RunData> RunDataToken;	
		edm::EDGetTokenT<WireChambers> MWCToken;
		edm::Service<TFileService> fs;
		bool writeMinimal;

	  	int run, pdgID, n_event, goodDWC_Measurement;
	  	double beamEnergy;
	  	double triggerTimeDiff;
	  	std::vector<double> time_DWC;
		std::vector<double> reco_x, reco_y;
		std::vector<double> res_x, res_y;
		std::vector<double> z;
	  	double x1_m_x2, x1_m_x3, x1_m_x4, x2_m_x3, x2_m_x4, x3_m_x4;
	  	double y1_m_y2, y1_m_y3, y1_m_y4, y2_m_y3, y2_m_y4, y3_m_y4;
	  	std::vector<int> dwc_Ntimestamps;
	  	std::vector<int> dwc_goodMeasurement;
	  	std::vector<int> dwc_goodMeasurementX;
	  	std::vector<int> dwc_goodMeasurementY;
	  	int N_goodMeasurements, N_goodMeasurements_X, N_goodMeasurements_Y;
	  	std::vector<double> residuals_x;
	  	std::vector<double> residuals_y;

	  	int NDF_x; double chi2_x, referenceError_x;
	  	int NDF_y; double chi2_y, referenceError_y;
		
		std::map<int, SensorHitMap*> Sensors;
		ParticleTrack* TrackFull;
		ParticleTrack* TracksUnbiased;  		
		TTree* tree;

};

DWC_NTupelizer::DWC_NTupelizer(const edm::ParameterSet& iConfig) {
	usesResource("TFileService");

	// initialization	
	MWCToken = consumes<WireChambers>(iConfig.getParameter<edm::InputTag>("MWCHAMBERS"));
	RunDataToken = consumes<RunData>(iConfig.getParameter<edm::InputTag>("RUNDATA"));

	writeMinimal = iConfig.getParameter<bool>("writeMinimal");

	tree = NULL;

	for (size_t i=0; i<4; i++) {
	  	time_DWC.push_back(-1.);
		reco_x.push_back(-1.); 
		reco_y.push_back(-1.);
		res_x.push_back(-1.); 
		res_y.push_back(-1.);
		z.push_back(-1.);
	  	dwc_Ntimestamps.push_back(0);
	  	dwc_goodMeasurement.push_back(0);
	  	dwc_goodMeasurementX.push_back(0);
	  	dwc_goodMeasurementY.push_back(0);
		residuals_x.push_back(-1.);
		residuals_y.push_back(-1.);
	}

}//constructor ends here

DWC_NTupelizer::~DWC_NTupelizer() {
	return;
}

// ------------ method called for each event  ------------
void DWC_NTupelizer::analyze(const edm::Event& event, const edm::EventSetup& setup) {
 	//get the relevant event information
	edm::Handle<RunData> rd;
	event.getByToken(RunDataToken, rd);

	//get the multi wire chambers
	edm::Handle<WireChambers> dwcs;
	event.getByToken(MWCToken, dwcs);
	
	run = rd->run;
	pdgID = rd->pdgID;
	beamEnergy = rd->energy;
	n_event = rd->event;
	goodDWC_Measurement = (rd->booleanUserRecords.has("hasValidDWCMeasurement")&&rd->booleanUserRecords.get("hasValidDWCMeasurement")) ? 1 : 0;
	
	triggerTimeDiff = rd->doubleUserRecords.has("triggerDeltaT_to_TDC") ? rd->doubleUserRecords.get("triggerDeltaT_to_TDC") : -1;

	for (size_t i=0; i<4; i++) {
		reco_x[i] = dwcs->at(i).x;
		reco_y[i] = dwcs->at(i).y;
		res_x[i] = dwcs->at(i).res_x;
		res_y[i] = dwcs->at(i).res_y;
		z[i] = dwcs->at(i).z;
		dwc_goodMeasurement[i] = dwcs->at(i).goodMeasurement;
	}

	if (!writeMinimal) {	
		N_goodMeasurements = N_goodMeasurements_X = N_goodMeasurements_Y = 0;
	 	
	 	x1_m_x2 = dwcs->at(0).x - dwcs->at(1).x;
	 	x1_m_x3 = dwcs->at(0).x - dwcs->at(2).x;
		x1_m_x4 = dwcs->at(0).x - dwcs->at(3).x;
		x2_m_x3 = dwcs->at(1).x - dwcs->at(2).x;
		x2_m_x4 = dwcs->at(1).x - dwcs->at(3).x;
		x3_m_x4 = dwcs->at(2).x - dwcs->at(3).x;
		y1_m_y2 = dwcs->at(0).y - dwcs->at(1).y;
		y1_m_y3 = dwcs->at(0).y - dwcs->at(2).y;
		y1_m_y4 = dwcs->at(0).y - dwcs->at(3).y;
		y2_m_y3 = dwcs->at(1).y - dwcs->at(2).y;
		y2_m_y4 = dwcs->at(1).y - dwcs->at(3).y;
		y3_m_y4 = dwcs->at(2).y - dwcs->at(3).y;

		for (size_t i=0; i<4; i++) {
		 	time_DWC[i] = dwcs->at(i).averagedTimeStamp;
			dwc_Ntimestamps[i] = dwcs->at(i).recordedTimeStamps;
			dwc_goodMeasurementX[i] = dwcs->at(i).goodMeasurement_X;
			dwc_goodMeasurementY[i] = dwcs->at(i).goodMeasurement_Y;
			
			if (dwc_goodMeasurement[i]) N_goodMeasurements++;
			if (dwc_goodMeasurementX[i]) N_goodMeasurements_X++;
			if (dwc_goodMeasurementY[i]) N_goodMeasurements_Y++;
		}

		//fits for x
		if (N_goodMeasurements>=4) {
			//first layer of HGCal as reference
			TrackFull = new ParticleTrack();
			Sensors[5] = new SensorHitMap(5);
			Sensors[5]->setLabZ(0., 0);
			Sensors[5]->setParticleEnergy(rd->energy);
			TrackFull->addReferenceSensor(Sensors[5]);
			
			for (size_t n_layer=0; n_layer<4; n_layer++) {	
				Sensors[n_layer] = new SensorHitMap(n_layer);				
				Sensors[n_layer]->setLabZ(dwcs->at(n_layer).z, 0.);
				Sensors[n_layer]->setCenterHitPosition(dwcs->at(n_layer).x, dwcs->at(n_layer).y , dwcs->at(n_layer).res_x, dwcs->at(n_layer).res_y);
				//std::cout<<n_layer<<": "<<dwcs->at(n_layer).y<<" good measurement ? "<<dwcs->at(n_layer).goodMeasurement_Y<<std::endl;

				Sensors[n_layer]->setParticleEnergy(rd->energy);
				//Sensors[n_layer]->setResidualResolution(dwcs->at(n_layer).res_x);	
				TrackFull->addFitPoint(Sensors[n_layer]);
			}
			
			TrackFull->fitTrack(LINEFITANALYTICAL);
			NDF_x = TrackFull->getNDF(1);
			NDF_y = TrackFull->getNDF(2);
			chi2_x = TrackFull->getChi2(1);
			chi2_y = TrackFull->getChi2(2);
			referenceError_x = TrackFull->calculateReferenceErrorXY().first;
			referenceError_y = TrackFull->calculateReferenceErrorXY().second;
			
			for (size_t n_layer=0; n_layer<4; n_layer++) {
				TracksUnbiased = new ParticleTrack();
				for (size_t i_layer=0; i_layer<4; i_layer++) {
					if (n_layer==i_layer) continue;
					TracksUnbiased->addFitPoint(Sensors[i_layer]);
				}
				TracksUnbiased->fitTrack(LINEFITANALYTICAL);
				residuals_x[n_layer] = Sensors[n_layer]->getHitPosition().first-TracksUnbiased->calculatePositionXY(Sensors[n_layer]->getLabZ(), 0).first;
				residuals_y[n_layer] = Sensors[n_layer]->getHitPosition().second-TracksUnbiased->calculatePositionXY(Sensors[n_layer]->getLabZ(), 0).second;
				delete TracksUnbiased;
			}
			
			for (std::map<int, SensorHitMap*>::iterator it=Sensors.begin(); it!=Sensors.end(); it++) {
				delete (*it).second;
			};	Sensors.clear();
			delete TrackFull;
		} else {
			NDF_x = NDF_y = 0;
			chi2_x = chi2_y = -999;
			referenceError_x = referenceError_y = -999;
			for (size_t n_layer=0; n_layer<4; n_layer++) residuals_x[n_layer] = residuals_y[n_layer] = -999;
		}
	}

	if ((rd->event==-1)&&writeMinimal) return;
	tree->Fill();
		

}// analyze ends here


void DWC_NTupelizer::beginJob() {	
	tree = fs->make<TTree>("dwc_reco", "dwc_reco");

	tree->Branch("run", &run);
	tree->Branch("pdgID", &pdgID);
	tree->Branch("beamEnergy", &beamEnergy);
	tree->Branch("event", &n_event);
	tree->Branch("triggerTimeDifference", &triggerTimeDiff);
	tree->Branch("goodDWC_Measurement", &goodDWC_Measurement);

	tree->Branch("reco1_x", &reco_x[0]);
	tree->Branch("reco1_y", &reco_y[0]);
	tree->Branch("res1_x", &res_x[0]);
	tree->Branch("res1_y", &res_y[0]);
	tree->Branch("z1", &z[0]);
	tree->Branch("dwc1_goodMeasurement", &dwc_goodMeasurement[0]);

	tree->Branch("reco2_x", &reco_x[1]);
	tree->Branch("reco2_y", &reco_y[1]);
	tree->Branch("res2_x", &res_x[1]);
	tree->Branch("res2_y", &res_y[1]);
	tree->Branch("z2", &z[1]);
	tree->Branch("dwc2_goodMeasurement", &dwc_goodMeasurement[1]);

	tree->Branch("reco3_x", &reco_x[2]);
	tree->Branch("reco3_y", &reco_y[2]);
	tree->Branch("res3_x", &res_x[2]);
	tree->Branch("res3_y", &res_y[2]);
	tree->Branch("z3", &z[2]);
	tree->Branch("dwc3_goodMeasurement", &dwc_goodMeasurement[2]);

	tree->Branch("reco4_x", &reco_x[3]);
	tree->Branch("reco4_y", &reco_y[3]);
	tree->Branch("res4_x", &res_x[3]);
	tree->Branch("res4_y", &res_y[3]);
	tree->Branch("z4", &z[3]);
	tree->Branch("dwc4_goodMeasurement", &dwc_goodMeasurement[3]); 


	if (!writeMinimal) {
	  	tree->Branch("time_DWC1", &time_DWC[0]);
	  	tree->Branch("time_DWC2", &time_DWC[1]);
	  	tree->Branch("time_DWC3", &time_DWC[2]);
	  	tree->Branch("time_DWC4", &time_DWC[3]);
	  	tree->Branch("x1_m_x2", &x1_m_x2);
	  	tree->Branch("x1_m_x3", &x1_m_x3);
	  	tree->Branch("x1_m_x4", &x1_m_x4);
	  	tree->Branch("x2_m_x3", &x2_m_x3);
	  	tree->Branch("x2_m_x4", &x2_m_x4);
	  	tree->Branch("x3_m_x4", &x3_m_x4);
	  	tree->Branch("y1_m_y2", &y1_m_y2);
	  	tree->Branch("y1_m_y3", &y1_m_y3);
	  	tree->Branch("y1_m_y4", &y1_m_y4);
	  	tree->Branch("y2_m_y3", &y2_m_y3);
	  	tree->Branch("y2_m_y4", &y2_m_y4);
	  	tree->Branch("y3_m_y4", &y3_m_y4);

		tree->Branch("dwc1_Ntimestamps", &dwc_Ntimestamps[0]);
		tree->Branch("dwc2_Ntimestamps", &dwc_Ntimestamps[1]);
		tree->Branch("dwc3_Ntimestamps", &dwc_Ntimestamps[2]);
		tree->Branch("dwc4_Ntimestamps", &dwc_Ntimestamps[3]);
		tree->Branch("dwc1_goodMeasurement_x", &dwc_goodMeasurementX[0]);
		tree->Branch("dwc2_goodMeasurement_x", &dwc_goodMeasurementX[1]);
		tree->Branch("dwc3_goodMeasurement_x", &dwc_goodMeasurementX[2]);
		tree->Branch("dwc4_goodMeasurement_x", &dwc_goodMeasurementX[3]); 
		tree->Branch("dwc1_goodMeasurement_y", &dwc_goodMeasurementY[0]);
		tree->Branch("dwc2_goodMeasurement_y", &dwc_goodMeasurementY[1]);
		tree->Branch("dwc3_goodMeasurement_y", &dwc_goodMeasurementY[2]);
		tree->Branch("dwc4_goodMeasurement_y", &dwc_goodMeasurementY[3]); 
		
		tree->Branch("N_goodMeasurements", &N_goodMeasurements);
		tree->Branch("N_goodMeasurements_x", &N_goodMeasurements_X);
		tree->Branch("N_goodMeasurements_y", &N_goodMeasurements_Y);
	
		tree->Branch("NDF_x", &NDF_x);
		tree->Branch("chi2_x", &chi2_x);
		tree->Branch("referenceError_x", &referenceError_x);

		tree->Branch("NDF_y", &NDF_y);
		tree->Branch("chi2_y", &chi2_y);
		tree->Branch("referenceError_y", &referenceError_y);

		tree->Branch("residual1_x", &residuals_x[0]);
		tree->Branch("residual1_y", &residuals_y[0]);
		tree->Branch("residual2_x", &residuals_x[1]);
		tree->Branch("residual2_y", &residuals_y[1]);
		tree->Branch("residual3_x", &residuals_x[2]);
		tree->Branch("residual3_y", &residuals_y[2]);
		tree->Branch("residual4_x", &residuals_x[3]);
		tree->Branch("residual4_y", &residuals_y[3]);
	}	
}

void DWC_NTupelizer::endJob() {

}

void DWC_NTupelizer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
	edm::ParameterSetDescription desc;
	desc.setUnknown();
	descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(DWC_NTupelizer);