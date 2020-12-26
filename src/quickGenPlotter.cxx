#include "AnalysisEvent.hpp"
#include "TChain.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TProfile.h"
#include "TLegend.h"
#include "TStyle.h"
#include "TASImage.h"
#include "TLatex.h"
#include "TMVA/Timer.h"
#include "TTree.h"
#include "TLorentzVector.h"
#include "TString.h"
#include "config_parser.hpp"

#include <boost/filesystem.hpp>
#include <boost/functional/hash.hpp>
#include <boost/numeric/conversion/cast.hpp>
#include <boost/program_options.hpp>
#include <boost/range/iterator_range.hpp>

#include <algorithm> 
#include <chrono> 
#include <fstream>
#include <iostream>
#include <regex>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <map>


std::vector<int> getLooseMuons(const AnalysisEvent& event);
bool getDileptonCand(AnalysisEvent& event, const std::vector<int>& muons, bool mcTruth = false);
bool scalarGrandparent(const AnalysisEvent& event, const Int_t& k, const Int_t& pdgId_);
float deltaR(float eta1, float phi1, float eta2, float phi2);

namespace fs = boost::filesystem;

// Lepton cut variables
const float looseMuonEta_ {2.8}, looseMuonPt_ {6.}, looseMuonPtLeading_ {15.}, looseMuonRelIso_ {100.};
const float invZMassCut_ {10.0};

int main(int argc, char* argv[])
{
    auto timerStart = std::chrono::high_resolution_clock::now(); 

    std::string config;
    std::vector<Dataset> datasets;
    double totalLumi;
    double usePreLumi;
    bool usePostLepTree {false};
   
    std::string outFileString{"plots/distributions/output.root"};
    bool is2016_;
    Long64_t nEvents;
    Long64_t totalEvents {0};
    const std::regex mask{".*\\.root"};

    // Gen Histos

    TH1F* h_genDimuonDeltaR       {new TH1F("h_genDimuonDeltaR",       "Dimuon gen deltaR", 500, 0., 10.)};
    TH1F* h_genDimuonMass         {new TH1F("h_genDimuonMass",         "Dimuon gen mass", 30, 0., 11.)};
    TH1F* h_genDimuonPt           {new TH1F("h_genDimuonPt",           "Dimuon gen Pt",  200, 0., 200)}; 
    TH1F* h_genDimuonEta          {new TH1F("h_genDimuonEta",          "Dimuon gen Eta", 60, 0., 5.)};
    TH1F* h_genLeadingMuonPt      {new TH1F("h_genLeadingMuonPt",      "Leading generator muon", 300, 0., 150.)};
    TH1F* h_genSubleadingMuonPt   {new TH1F("h_genSubleadingMuonPt",   "Subleading generator muon", 300, 0., 150.)};

    TH1F* h_genDipionDeltaR       {new TH1F("h_genDipionDeltaR",       "Dipion gen deltaR", 500, 0., 10.)};
    TH1F* h_genDipionMass         {new TH1F("h_genDipionMass",         "Dipion gen mass", 30, 0., 11.)};
    TH1F* h_genDipionPt           {new TH1F("h_genDipionPt",           "Dipion gen Pt",  200, 0., 200)}; 
    TH1F* h_genDipionEta          {new TH1F("h_genDipionEta",          "Dipion gen Eta", 60, 0., 5.)};
    TH1F* h_genLeadingPionPt      {new TH1F("h_genLeadingPionPt",      "Leading generator Pion", 300, 0., 150.)};
    TH1F* h_genSubleadingPionPt   {new TH1F("h_genSubleadingPionPt",   "Subleading generator Pion", 300, 0., 150.)};

    TH1F* h_genDikaonDeltaR       {new TH1F("h_genDikaonDeltaR",       "Dikaon gen deltaR", 500, 0., 10.)};
    TH1F* h_genDikaonMass         {new TH1F("h_genDikaonMass",         "Dikaon gen mass", 30, 0., 11.)};
    TH1F* h_genDikaonPt           {new TH1F("h_genDikaonPt",           "Dikaon gen Pt",  200, 0., 200)}; 
    TH1F* h_genDikaonEta          {new TH1F("h_genDikaonEta",          "Dikaon gen Eta", 60, 0., 5.)};
    TH1F* h_genLeadingKaonPt      {new TH1F("h_genLeadingKaonPt",      "Leading generator Kaon", 300, 0., 150.)};
    TH1F* h_genSubleadingKaonPt   {new TH1F("h_genSubleadingKaonPt",   "Subleading generator Kaon", 300, 0., 150.)};

    // Reco plots

    TH1F* h_recoDimuonDeltaR        {new TH1F("h_recoDimuonDeltaR",      "Dimuon reco deltaR", 500, 0., 10.)};
    TH1F* h_recoDimuonMass          {new TH1F("h_recoDimuonMass",        "Dimuon reco mass", 30, 0., 11.)};
    TH1F* h_recoDimuonPt            {new TH1F("h_recoDimuonPt",          "Dimuon reco Pt",  200, 0., 200)}; 
    TH1F* h_recoDimuonEta           {new TH1F("h_recoDimuonEta",         "Dimuon reco Eta", 60, 0., 5.)};
    TH1F* h_recoLeadingMuonPt       {new TH1F("h_recoLeadingMuonPt",     "Leading generator muon", 300, 0., 150.)};
    TH1F* h_recoSubleadingMuonPt    {new TH1F("h_recoSubleadingMuonPt",  "Subleading generator muon", 300, 0., 150.)};

    TProfile* p_numPromptMuons            {new TProfile ("p_numPromptMuons", "Muon flags for selected reco muons", 3, 0.5, 3.5, "ymax = 1.0")};
    p_numPromptMuons->GetXaxis()->SetBinLabel(1, "Prompt Decayed");
    p_numPromptMuons->GetXaxis()->SetBinLabel(2, "Prompt Final State");
    p_numPromptMuons->GetXaxis()->SetBinLabel(3, "Hard Process");

    TProfile* p_recoSelectedMuonMatching  {new TProfile ("p_recoSelectedMuonMatching", "Selected leading/subleading reco muon ancestry", 4, 0.5, 4.5, "ymax = 1.0")};
    p_recoSelectedMuonMatching->GetXaxis()->SetBinLabel(1, "Both muon tracks genuine");
    p_recoSelectedMuonMatching->GetXaxis()->SetBinLabel(2, "Leading muon track geunine");
    p_recoSelectedMuonMatching->GetXaxis()->SetBinLabel(3, "Subleading muon track genuine");
    p_recoSelectedMuonMatching->GetXaxis()->SetBinLabel(4, "Both genuine tracks fake");

    TH1F* h_recoGenDimuonDeltaR        {new TH1F("h_recoGenDimuonDeltaR",     "Dimuon recoGen deltaR", 500, 0., 10.)};
    TH1F* h_recoGenDimuonMass          {new TH1F("h_recoGenDimuonMass",       "Dimuon recoGen mass", 30, 0., 11.)};
    TH1F* h_recoGenDimuonPt            {new TH1F("h_recoGenDimuonPt",         "Dimuon recoGen Pt",  200, 0., 200)}; 
    TH1F* h_recoGenDimuonEta           {new TH1F("h_recoGenDimuonEta",        "Dimuon recoGen Eta", 60, 0., 5.)};
    TH1F* h_recoGenLeadingMuonPt       {new TH1F("h_recoGenLeadingMuonPt",    "Leading recoGen muon", 300, 0., 150.)};
    TH1F* h_recoGenSubleadingMuonPt    {new TH1F("h_recoGenSubleadingMuonPt", "Subleading recoGen muon", 300, 0., 150.)};

    TProfile* p_fakeSelected           {new TProfile("p_fakeSelected",  "Debug info for why fake #mu#mu pair was selected", 8, 0.5, 8.5, "ymax = 1.0")};
    p_fakeSelected->GetXaxis()->SetBinLabel(1, "0 #mu from a in event");
    p_fakeSelected->GetXaxis()->SetBinLabel(2, "1 #mu from a in event");
    p_fakeSelected->GetXaxis()->SetBinLabel(3, "2 #mu from a in event");
    p_fakeSelected->GetXaxis()->SetBinLabel(4, "2+ #mu from a in event");
    p_fakeSelected->GetXaxis()->SetBinLabel(5, "0 #mu from #mu in event");
    p_fakeSelected->GetXaxis()->SetBinLabel(6, "1 #mu from #mu in event");
    p_fakeSelected->GetXaxis()->SetBinLabel(7, "2 #mu from #mu in event");
    p_fakeSelected->GetXaxis()->SetBinLabel(8, "2+ #mu from #mu in event");

    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()("help,h", "Print this message.")(
        "config,c",
        po::value<std::string>(&config)->required(),
        "The configuration file to be used.")(
        "lumi,l",
        po::value<double>(&usePreLumi)->default_value(41528.0),
        "Lumi to scale MC plots to.")(
        "outfile,o",
        po::value<std::string>(&outFileString)->default_value(outFileString),
        "Output file for plots.")(
        ",n",
        po::value<Long64_t>(&nEvents)->default_value(0),
        "The number of events to be run over. All if set to 0.")(
        ",u",
        po::bool_switch(&usePostLepTree),
        "Use post lepton selection trees.")(
        "2016", po::bool_switch(&is2016_), "Use 2016 conditions (SFs, et al.).");
    po::variables_map vm;

    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help"))
        {
            std::cout << desc;
            return 0;
        }

        po::notify(vm);
    }
    catch (po::error& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }

    // Some vectors that will be filled in the parsing
    totalLumi = 0;

    try
    {
        Parser::parse_config(config,
                             datasets,
                             totalLumi,
                             usePostLepTree);
    }
    catch (const std::exception)
    {
        std::cerr << "ERROR Problem with a confugration file, see previous "
                     "errors for more details. If this is the only error, the "
                     "problem is with the main configuration file."
                  << std::endl;
        throw;
    }

    if (totalLumi == 0.)
    {
        totalLumi = usePreLumi;
    }
    std::cout << "Using lumi: " << totalLumi << std::endl;

    bool datasetFilled{false};
    const std::string postLepSelSkimInputDir{std::string{"/pnfs/iihe/cms/store/user/almorton/MC/postLepSkims/postLepSkims"} + (is2016_ ? "2016" : "2017") + "/"};
//    const std::string postLepSelSkimInputDir{std::string{"/user/almorton/HToSS_analysis/postLepSkims"} + (is2016_ ? "2016" : "2017") + "/"};

    // Begin to loop over all datasets
    for (auto dataset = datasets.begin(); dataset != datasets.end(); ++dataset)
    {
        datasetFilled = false;
        TChain* datasetChain{new TChain{dataset->treeName().c_str()}};
        datasetChain->SetAutoSave(0);

        std::cerr << "Processing dataset " << dataset->name() << std::endl;
        if (!usePostLepTree) {
            if (!datasetFilled) {
                if (!dataset->fillChain(datasetChain)) {
                    std::cerr << "There was a problem constructing the chain for " << dataset->name() << ". Continuing with next dataset.\n";
                    continue;
                }
                datasetFilled=true;
            }
        }
        else {
            std::cout << postLepSelSkimInputDir + dataset->name() + "mumuSmallSkim.root" << std::endl;
            datasetChain->Add((postLepSelSkimInputDir + dataset->name() + "mumuSmallSkim.root").c_str());
        }

        // extract the dataset weight. MC = (lumi*crossSection)/(totalEvents), data = 1.0
        float datasetWeight{dataset->getDatasetWeight(totalLumi)};
        std::cout << datasetChain->GetEntries() << " number of items in tree. Dataset weight: " << datasetWeight << std::endl;
        if (datasetChain->GetEntries() == 0) {
            std::cout << "No entries in tree, skipping..." << std::endl;
            continue;
        }

        AnalysisEvent event{dataset->isMC(), datasetChain, is2016_};

        Long64_t numberOfEvents{datasetChain->GetEntries()};
        if (nEvents && nEvents < numberOfEvents) numberOfEvents = nEvents;

        TMVA::Timer* lEventTimer{ new TMVA::Timer{boost::numeric_cast<int>(numberOfEvents), "Running over dataset ...", false}}; 
        lEventTimer->DrawProgressBar(0, "");
    
        totalEvents += numberOfEvents;
        for (Long64_t i{0}; i < numberOfEvents; i++) {

            lEventTimer->DrawProgressBar(i,"");

            event.GetEntry(i);

            float eventWeight = 1.;
             eventWeight *= datasetWeight;

            std::vector<int> genMuonIndex;
            std::vector<int> genPionIndex;
            std::vector<int> genKaonIndex;

            // gen particle loop
            for ( Int_t k = 0; k < event.nGenPar; k++ ) {
                const int pid { std::abs(event.genParId[k]) };
                const int motherId { std::abs(event.genParMotherId[k]) };
                if ( pid == 13 && motherId == 9000006) {
                    genMuonIndex.emplace_back(k);
                }
                else if ( pid == 211 && motherId < 6 && motherId > 0 ) {
                    if ( scalarGrandparent(event, k, 9000006) ) genPionIndex.emplace_back(k);
                }
                else if ( pid == 321  && motherId < 6 && motherId > 0 ) {
                    if ( scalarGrandparent(event, k, 9000006) ) genKaonIndex.emplace_back(k);
                }
            }

            if ( genMuonIndex.size() == 2 ) {
                bool firstLeading {false};
                if ( event.genParPt[genMuonIndex[0]] > event.genParPt[genMuonIndex[1]] ) firstLeading = true;
                TLorentzVector genMuon1, genMuon2;
                if (firstLeading) {
                    genMuon1.SetPtEtaPhiE(event.genParPt[genMuonIndex[0]], event.genParEta[genMuonIndex[0]], event.genParPhi[genMuonIndex[0]], event.genParE[genMuonIndex[0]]);
                    genMuon2.SetPtEtaPhiE(event.genParPt[genMuonIndex[1]], event.genParEta[genMuonIndex[1]], event.genParPhi[genMuonIndex[1]], event.genParE[genMuonIndex[1]]);
                    h_genDimuonDeltaR->Fill( genMuon1.DeltaR(genMuon2) );
                    h_genDimuonMass->Fill( (genMuon1+genMuon2).M() );
      	       	    h_genDimuonPt->Fill( (genMuon1+genMuon2).Pt() );
      	       	    h_genDimuonEta->Fill( (genMuon1+genMuon2).Eta() );
                }
                else {
                    genMuon1.SetPtEtaPhiE(event.genParPt[genMuonIndex[1]], event.genParEta[genMuonIndex[1]], event.genParPhi[genMuonIndex[1]], event.genParE[genMuonIndex[1]]);
                    genMuon2.SetPtEtaPhiE(event.genParPt[genMuonIndex[0]], event.genParEta[genMuonIndex[0]], event.genParPhi[genMuonIndex[0]], event.genParE[genMuonIndex[0]]);
                    h_genDimuonDeltaR->Fill( genMuon1.DeltaR(genMuon2) );
                    h_genDimuonMass->Fill( (genMuon1+genMuon2).M() );
      	       	    h_genDimuonPt->Fill( (genMuon1+genMuon2).Pt() );
      	       	    h_genDimuonEta->Fill( (genMuon1+genMuon2).Eta() );
                }
            }

            if ( genPionIndex.size() > 2 ) {
                std::cout << "genPionIndex.size(): " << genPionIndex.size() << std::endl;
            }

            if ( genKaonIndex.size() > 2 ) {
                std::cout << "genKaonIndex.size(): " << genKaonIndex.size() << std::endl;
            }

            const bool passSingleMuonTrigger {event.muTrig()}, passDimuonTrigger {event.mumuTrig()};
            const bool passL2MuonTrigger {event.mumuL2Trig()}, passDimuonNoVtxTrigger {event.mumuNoVtxTrig()};

            if (! ( passDimuonTrigger || passSingleMuonTrigger || passL2MuonTrigger || passDimuonNoVtxTrigger ) ) continue;
            if (!event.metFilters()) continue;
 
            event.muonIndexLoose = getLooseMuons(event);

            if ( event.muonIndexLoose.size() < 2 ) continue;

            if ( !getDileptonCand( event, event.muonIndexLoose ) ) continue;

            h_recoDimuonDeltaR->Fill(event.zPairLeptons.first.DeltaR(event.zPairLeptons.second));
            h_recoDimuonMass->Fill( (event.zPairLeptons.first + event.zPairLeptons.second).M() );
            h_recoDimuonPt->Fill( (event.zPairLeptons.first + event.zPairLeptons.second).Pt() );
            h_recoDimuonEta->Fill((event.zPairLeptons.first + event.zPairLeptons.second).Eta() );
            h_recoLeadingMuonPt->Fill( (event.zPairLeptons.first).Pt() );
            h_recoSubleadingMuonPt->Fill( (event.zPairLeptons.second).Pt() );

            const bool genuineRecoMuon1 {std::abs(event.genMuonPF2PATMotherId[event.zPairIndex.first]) == 9000006}, genuineRecoMuon2 {std::abs(event.genMuonPF2PATMotherId[event.zPairIndex.second]) == 9000006};

            p_numPromptMuons->Fill( 1.0, event.genMuonPF2PATPromptDecayed[event.zPairIndex.first] );
            p_numPromptMuons->Fill( 1.0, event.genMuonPF2PATPromptDecayed[event.zPairIndex.second] );
            p_numPromptMuons->Fill( 2.0, event.genMuonPF2PATPromptFinalState[event.zPairIndex.first] );
            p_numPromptMuons->Fill( 2.0, event.genMuonPF2PATPromptFinalState[event.zPairIndex.second] );
            p_numPromptMuons->Fill( 3.0, event.genMuonPF2PATHardProcess[event.zPairIndex.first] );
            p_numPromptMuons->Fill( 3.0, event.genMuonPF2PATHardProcess[event.zPairIndex.second] );

            p_recoSelectedMuonMatching->Fill( 1.0, bool (genuineRecoMuon1 && genuineRecoMuon2) );
            p_recoSelectedMuonMatching->Fill( 2.0, bool (genuineRecoMuon1 && !genuineRecoMuon2) );
            p_recoSelectedMuonMatching->Fill( 3.0, bool (!genuineRecoMuon1 && genuineRecoMuon2) );
            p_recoSelectedMuonMatching->Fill( 4.0, bool (!genuineRecoMuon1 && !genuineRecoMuon2) );

            // Look at loose muons that are matched to MC truth only
            if ( !getDileptonCand( event, event.muonIndexLoose, true ) ) continue;

            h_recoGenDimuonDeltaR->Fill(event.zPairLeptons.first.DeltaR(event.zPairLeptons.second));
            h_recoGenDimuonMass->Fill( (event.zPairLeptons.first + event.zPairLeptons.second).M() );
            h_recoGenDimuonPt->Fill( (event.zPairLeptons.first + event.zPairLeptons.second).Pt() );
            h_recoGenDimuonEta->Fill((event.zPairLeptons.first + event.zPairLeptons.second).Eta() );
            h_recoGenLeadingMuonPt->Fill( (event.zPairLeptons.first).Pt() );
            h_recoGenSubleadingMuonPt->Fill( (event.zPairLeptons.second).Pt() );

            if ( !genuineRecoMuon1 && !genuineRecoMuon2 ) {
                unsigned int nGenuineMuons {0}, nMuonsFromMuons{0};
                for ( auto it = event.muonIndexLoose.begin(); it != event.muonIndexLoose.end(); it++ ){
                    if ( std::abs(event.genMuonPF2PATMotherId[*it]) == 9000006 ) nGenuineMuons++;
                    if ( std::abs(event.genMuonPF2PATMotherId[*it]) == 13 ) nMuonsFromMuons++;
                }
                p_fakeSelected->Fill( 1.0, bool ( nGenuineMuons == 0 ) );
                p_fakeSelected->Fill( 2.0, bool ( nGenuineMuons == 1 ) );
                p_fakeSelected->Fill( 3.0, bool ( nGenuineMuons == 2 ) );
                p_fakeSelected->Fill( 4.0, bool ( nGenuineMuons > 2 ) );
                p_fakeSelected->Fill( 5.0, bool ( nMuonsFromMuons == 0 ) );
                p_fakeSelected->Fill( 6.0, bool ( nMuonsFromMuons == 1 ) );
                p_fakeSelected->Fill( 7.0, bool ( nMuonsFromMuons == 2 ) );
                p_fakeSelected->Fill( 8.0, bool ( nMuonsFromMuons > 2 ) );
            }

        } // end event loop
    } // end dataset loop

    TFile* outFile{new TFile{outFileString.c_str(), "RECREATE"}};
    outFile->cd();

    h_genDimuonDeltaR->Write();
    h_genDimuonMass->Write();
    h_genDimuonPt->Write();
    h_genDimuonEta->Write();
    h_genLeadingMuonPt->Write();
    h_genSubleadingMuonPt->Write();

    h_genDipionDeltaR->Write();
    h_genDipionMass->Write();
    h_genDipionPt->Write();
    h_genDipionEta->Write();
    h_genLeadingPionPt->Write();
    h_genSubleadingPionPt->Write();

    h_genDikaonDeltaR->Write();
    h_genDikaonMass->Write();
    h_genDikaonPt->Write();
    h_genDikaonEta->Write();
    h_genLeadingKaonPt->Write();
    h_genSubleadingKaonPt->Write();

    h_recoDimuonDeltaR->Write();
    h_recoDimuonMass->Write();
    h_recoDimuonPt->Write();
    h_recoDimuonEta->Write();
    h_recoLeadingMuonPt->Write();
    h_recoSubleadingMuonPt->Write();

    p_numPromptMuons->Write();
    p_recoSelectedMuonMatching->Write();

    h_recoGenDimuonDeltaR->Write();
    h_recoGenDimuonMass->Write();
    h_recoGenDimuonPt->Write();
    h_recoGenDimuonEta->Write();
    h_recoGenLeadingMuonPt->Write();
    h_recoGenSubleadingMuonPt->Write();

    p_fakeSelected->Write();

    outFile->Close();

//    std::cout << "Max nGenPar: " << maxGenPars << std::endl;    
    auto timerStop = std::chrono::high_resolution_clock::now(); 
    auto duration  = std::chrono::duration_cast<std::chrono::seconds>(timerStop - timerStart);

    std::cout << "\nFinished. Took " << duration.count() << " seconds" <<std::endl;
}

std::vector<int> getLooseMuons(const AnalysisEvent& event) {
    std::vector<int> muons;
    for (int i{0}; i < event.numMuonPF2PAT; i++)  {
       if (event.muonPF2PATIsPFMuon[i] && event.muonPF2PATLooseCutId[i] /*&& event.muonPF2PATPfIsoLoose[i]*/ && std::abs(event.muonPF2PATEta[i]) < looseMuonEta_) {
           if (event.muonPF2PATPt[i] >= (muons.empty() ? looseMuonPtLeading_ : looseMuonPt_)) muons.emplace_back(i);
        }
    }
    return muons;
}

bool getDileptonCand(AnalysisEvent& event, const std::vector<int>& muons, bool mcTruth) {

    double maxDeltaR {10.};
    for ( unsigned int i{0}; i < muons.size(); i++ ) {
        for ( unsigned int j{i+1}; j < muons.size(); j++ ) {
            if (event.muonPF2PATCharge[i] * event.muonPF2PATCharge[j] >= 0) continue;
            if ( mcTruth && event.genMuonPF2PATMotherId[i] == 9000006 && event.genMuonPF2PATMotherId[j] == 9000006) continue;
            TLorentzVector lepton1{event.muonPF2PATPX[i], event.muonPF2PATPY[i], event.muonPF2PATPZ[i], event.muonPF2PATE[i]};
            TLorentzVector lepton2{event.muonPF2PATPX[j], event.muonPF2PATPY[j], event.muonPF2PATPZ[j], event.muonPF2PATE[j]};
            double delR { lepton1.DeltaR(lepton2) };
            if ( delR < maxDeltaR  ) {
                event.zPairLeptons.first  = lepton1.Pt() > lepton2.Pt() ? lepton1 : lepton2;
                event.zPairLeptons.second = lepton1.Pt() > lepton2.Pt() ? lepton2 : lepton1;
                event.zPairIndex.first = lepton1.Pt() > lepton2.Pt() ? muons[i] : muons[j];
                event.zPairIndex.second  = lepton1.Pt() > lepton2.Pt() ? muons[j] : muons[i];
                event.zPairRelIso.first  = event.muonPF2PATComRelIsodBeta[muons[i]];
                event.zPairRelIso.second = event.muonPF2PATComRelIsodBeta[muons[j]];
                return true;
            }
        }
    }
    return false;
}


bool scalarGrandparent (const AnalysisEvent& event, const Int_t& k, const Int_t& grandparentId) {

    const Int_t pdgId        { std::abs(event.genParId[k]) };
    const Int_t numDaughters { event.genParNumDaughters[k] };
    const Int_t motherId     { std::abs(event.genParMotherId[k]) };
    const Int_t motherIndex  { std::abs(event.genParMotherIndex[k]) };


    if (motherId == 0 || motherIndex == -1) return false; // if no parent, then mother Id is null and there's no index, quit search
    else if (motherId == std::abs(grandparentId)) return true; // if mother is granparent being searched for, return true
    else if (motherIndex >= event.NGENPARMAX) return false; // index exceeds stored genParticle range, return false for safety
    else {
//        std::cout << "Going up the ladder ... pdgId = " << pdgId << " : motherIndex = " << motherIndex << " : motherId = " << motherId << std::endl;
//        debugCounter++;
//        std::cout << "debugCounter: " << debugCounter << std::endl;
        return scalarGrandparent(event, motherIndex, grandparentId); // otherwise check mother's mother ...
    }
}

float deltaR(float eta1, float phi1, float eta2, float phi2){
  float dEta = eta1-eta2;
  float dPhi = phi1-phi2;
  while (fabs(dPhi) > 3.14159265359){
    dPhi += (dPhi > 0.? -2*3.14159265359:2*3.14159265359);
  }
  //  if(singleEventInfoDump_)  std::cout << eta1 << " " << eta2 << " phi " << phi1 << " " << phi2 << " ds: " << eta1-eta2 << " " << phi1-phi2 << " dR: " << std::sqrt((dEta*dEta)+(dPhi*dPhi)) << std::endl;
  return std::sqrt((dEta*dEta)+(dPhi*dPhi));
}

