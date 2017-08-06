#ifndef _triggerScaleFactorsAlgo_hpp_
#define _triggerScaleFactorsAlgo_hpp_

#include <vector>
#include "TH1.h"
#include "TH2.h"
#include "TCanvas.h"
#include "TPad.h"

#include "dataset.hpp"

#include <libconfig.h++>
#include <LHAPDF/LHAPDF.h>

#include <map>

class AnalysisEvent;
class TTree;
class TFile;

class TriggerScaleFactors{

	public:
        // Constructor
        TriggerScaleFactors();
        ~TriggerScaleFactors();

	void setBranchStatusAll(TTree * chain, bool isMC, std::string triggerFlag);
	void show_usage(std::string name);

	void parseCommandLineArguements (int argc, char* argv[]);
	void runMainAnalysis();
	void savePlots();

	private:

	std::string config;
	double usePreLumi;
	long nEvents;
	std::string outFolder;
	std::string postfix;
	int numFiles;

	std::vector<Dataset> datasets;
	double totalLumi;
	double* lumiPtr;

	bool is2016_;

	// PU reweighting
	TFile * dataPileupFile;
	TH1F* dataPU;
	TFile * mcPileupFile;
	TH1F* mcPU;
	TFile * systUpFile;
	TH1F* pileupUpHist;
	TFile * systDownFile;
	TH1F* pileupDownHist;
	TH1F* puReweight;
	TH1F* puSystUp;
	TH1F* puSystDown;

	// lepton selection
	std::vector<int> getTightElectrons(AnalysisEvent*);
	std::vector<int> getTightMuons(AnalysisEvent*);
	bool passDileptonSelection(AnalysisEvent*,int);

	// trigger cuts
	bool doubleElectronTriggerCut(AnalysisEvent*, bool);
	bool muonElectronTriggerCut(AnalysisEvent*, bool);
	bool doubleMuonTriggerCut(AnalysisEvent*, bool);
	bool metTriggerCut(AnalysisEvent*);
	bool metFilters(AnalysisEvent*,bool);

	//Efficiencies
	double numberPassedElectrons[2];
	double numberTriggeredDoubleElectrons[2];

	double numberPassedMuons[2];
	double numberTriggeredDoubleMuons[2];

	double numberPassedMuonElectrons[2];
	double numberTriggeredMuonElectrons[2];

	//Systematic variables
	double numberSelectedElectrons[2];
	double numberSelectedMuons[2];
	double numberSelectedMuonElectrons[2];

        double numberSelectedDoubleElectronsTriggered[2];
	double numberSelectedDoubleMuonsTriggered[2];
	double numberSelectedMuonElectronsTriggered[2]; // Double MuonEG

};

#endif

