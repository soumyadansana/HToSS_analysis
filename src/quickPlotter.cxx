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
std::vector<int> getPromptMuons(const AnalysisEvent& event, const std::vector<int>& muonIndex, const bool getPrompt );
bool getDileptonCand(AnalysisEvent& event, const std::vector<int>& muons, const float scalarMass, const bool exactlyTwo);
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
    float scalarMass_;
    bool exactlyTwo_ {false};
   
    std::string outFileString{"plots/distributions/output.root"};
    bool is2016_;
    Long64_t nEvents;
    Long64_t totalEvents {0};
    const std::regex mask{".*\\.root"};

    // Gen Histos

    TProfile* p_kaonAncestry           {new TProfile("p_kaonAncestry", "Intermediate ancestry of charged kaons from scalar decays", 5, 0.5, 5.5)};

    p_kaonAncestry->GetXaxis()->SetBinLabel(1, "No D meson");
    p_kaonAncestry->GetXaxis()->SetBinLabel(2, "D inc");
    p_kaonAncestry->GetXaxis()->SetBinLabel(3, "D#pm");
    p_kaonAncestry->GetXaxis()->SetBinLabel(4, "D0");
    p_kaonAncestry->GetXaxis()->SetBinLabel(5, "D^{#pm}_{S}");

    // Quick and dirty trigger plots
    // denom
    TH1F* h_leadingMuonPt_truth            {new TH1F("h_leadingMuonPt_truth",      "", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_truth         {new TH1F("h_subLeadingMuonPt_truth",   "", 200, 0., 100.)};
    TH1F* h_leadingMuonPt                  {new TH1F("h_leadingMuonPt",            "", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt               {new TH1F("h_subLeadingMuonPt",         "", 200, 0., 100.)};
    TH1F* h_delR_truth                     {new TH1F("h_delR_truth",               "", 100, 0., 1.0)};
    TH1F* h_delR                           {new TH1F("h_delR",                     "", 100, 0., 1.0)};
    // numerator - single mu
    TH1F* h_leadingMuonPt_truth_muTrig     {new TH1F("h_leadingMuonPt_truth_muTrig",      "Trigger turn-on for signal; p_{T} (GeV); #mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_truth_muTrig  {new TH1F("h_subLeadingMuonPt_truth_muTrig",   "Trigger turn-on for signal; p_{T} (GeV); #mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_leadingMuonPt_muTrig           {new TH1F("h_leadingMuonPt_muTrig",            "Trigger turn-on for signal; p_{T} (GeV); #mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_muTrig        {new TH1F("h_subLeadingMuonPt_muTrig",         "Trigger turn-on for signal; p_{T} (GeV); #mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_delR_truth_muTrig              {new TH1F("h_delR_truth_muTrig",               "Trigger turn-on for signal; p_{T} (GeV); #Delta R", 100, 0., 1.0)};
    TH1F* h_delR_muTrig                    {new TH1F("h_delR_muTrig",                     "Trigger turn-on for signal; p_{T} (GeV); #Delta R", 100, 0., 1.0)};
    // numerator - doubele mu
    TH1F* h_leadingMuonPt_truth_mumuTrig       {new TH1F("h_leadingMuonPt_truth_mumuTrig",      "Trigger turn-on for signal; p_{T} (GeV); #mu#mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_truth_mumuTrig    {new TH1F("h_subLeadingMuonPt_truth_mumuTrig",   "Trigger turn-on for signal; p_{T} (GeV); #mu#mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_leadingMuonPt_mumuTrig             {new TH1F("h_leadingMuonPt_mumuTrig",            "Trigger turn-on for signal; p_{T} (GeV); #mu#mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_mumuTrig          {new TH1F("h_subLeadingMuonPt_mumuTrig",         "Trigger turn-on for signal; p_{T} (GeV); #mu#mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_delR_truth_mumuTrig                {new TH1F("h_delR_truth_mumuTrig",               "Trigger turn-on for signal; p_{T} (GeV); #Delta R", 100, 0., 1.0)};
    TH1F* h_delR_mumuTrig                      {new TH1F("h_delR_mumuTrig",                    "Trigger turn-on for signal; p_{T} (GeV); #Delta R", 100, 0., 1.0)};
    // numerator - L2 mu
    TH1F* h_leadingMuonPt_truth_L2muTrig       {new TH1F("h_leadingMuonPt_truth_L2muTrig",      "Trigger turn-on for signal; p_{T} (GeV); L2 #mu#mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_truth_L2muTrig    {new TH1F("h_subLeadingMuonPt_truth_L2muTrig",   "Trigger turn-on for signal; p_{T} (GeV); L2 #mu#mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_leadingMuonPt_L2muTrig             {new TH1F("h_leadingMuonPt_L2muTrig",            "Trigger turn-on for signal; p_{T} (GeV); L2 #mu#mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_subLeadingMuonPt_L2muTrig          {new TH1F("h_subLeadingMuonPt_L2muTrig",         "Trigger turn-on for signal; p_{T} (GeV); L2 #mu#mu trigger #epislon", 200, 0., 100.)};
    TH1F* h_delR_truth_L2muTrig                {new TH1F("h_delR_truth_L2muTrig",               "Trigger turn-on for signal; p_{T} (GeV); #Delta R", 100, 0., 1.0)};
    TH1F* h_delR_L2muTrig                      {new TH1F("h_delR_L2muTrig",                    "Trigger turn-on for signal; p_{T} (GeV); #Delta R", 100, 0., 1.0)};

    // Reco Muon Histos
    TH1F* h_numMuons                   {new TH1F("h_numMuons",                 "number of reco muons in the event", 21, -0.5, 20.5)};
    TH1F* h_numMuonsFromScalar         {new TH1F("h_numMuonsFromScalar",       "number of reco muons with motherId = scalarId", 21, -0.5, 20.5)};
    TH1F* h_numMuonsPromptFinalState   {new TH1F("h_numMuonsPromptFinalState", "number of reco muons with PromptFinalState flag", 21, -0.5, 20.5)};
    TH1F* h_numMuonsHardProcess        {new TH1F("h_numMuonsHardProcess",      "number of reco muons with HardProcess flag", 21, -0.5, 20.5)};

    TProfile* p_hardMuonsIdProfile         {new TProfile("p_hardMuonsIdProfile", "ID decisions for muons from hard process", 9, 0.5, 9.5)};
    TProfile* p_softMuonsIdProfile         {new TProfile("p_softMuonsIdProfile", "ID decisions for muons not from hard process", 9, 0.5, 9.5)};

    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(1, "Pass PF ID");
    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(2, "Pass Loose Cut ID");
    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(3, "Pass Medium Cut ID");
    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(4, "Pass Tight Cut ID");
    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(5, "Pass Very Loose Iso Cut");
    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(6, "Pass Loose Iso Cut");
    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(7, "Pass Medium Iso Cut");
    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(8, "Pass Tight Iso Cut");
    p_hardMuonsIdProfile->GetXaxis()->SetBinLabel(9, "Pass Very Tight Iso Cut");

    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(1, "Pass PF ID");
    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(2, "Pass Loose Cut ID");
    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(3, "Pass Medium Cut ID");
    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(4, "Pass Tight Cut ID");
    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(5, "Pass Very Loose Iso Cut");
    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(6, "Pass Loose Iso Cut");
    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(7, "Pass Medium Iso Cut");
    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(8, "Pass Tight Iso Cut");
    p_softMuonsIdProfile->GetXaxis()->SetBinLabel(9, "Pass Very Tight Iso Cut");

    TH1F* h_promptMuonPt               {new TH1F("h_promptMuonPt",       "prompt muon p_{T}; p_{T} (GeV);", 400, 0., 200.)};
    TH1F* h_promptMuonEta              {new TH1F("h_promptMuonEta",      "prompt muon #eta; #eta;", 120, 0., 3.2)};
    TH1F* h_promptMuonPhi              {new TH1F("h_promptMuonPhi",      "prompt muon #phi; #phi;", 300, -3.2, 3.2)};
    TH1F* h_promptMuonRelIso           {new TH1F("h_promptMuonRelIso",   "prompt muon rel iso; rel iso;", 500, 0., 1.)};
    TH1F* h_promptMuonDz               {new TH1F("h_promptMuonDz",       "prompt muon d_{z}; d_{z} (cm);", 500, 0., 1.)};
    TH1F* h_promptMuonDxy              {new TH1F("h_promptMuonDxy",      "prompt muon d_{xy}; d_{xy} (cm);", 500, 0., 1.)};
    TH1F* h_promptMuonPdgId            {new TH1F("h_promptMuonPdgId",    "prompt muon matched genPar Id", 501, -0.5, 500.5)};
    TH1F* h_promptMuonMotherId         {new TH1F("h_promptMuonMotherId", "prompt muon matched genPar mother Id", 502, -1.5, 500.5)};

    TH1F* h_nonpromptMuonPt            {new TH1F("h_nonpromptMuonPt",       "nonprompt muon p_{T}; p_{T} (GeV);", 400, 0., 200.)};
    TH1F* h_nonpromptMuonEta           {new TH1F("h_nonpromptMuonEta",      "nonprompt muon #eta; #eta;", 120, 0., 3.2)};
    TH1F* h_nonpromptMuonPhi           {new TH1F("h_nonpromptMuonPhi",      "nonprompt muon #phi; #phi;", 300, -3.2, 3.2)};
    TH1F* h_nonpromptMuonRelIso        {new TH1F("h_nonpromptMuonRelIso",   "nonprompt muon rel iso; rel iso;", 500, 0., 1.)};
    TH1F* h_nonpromptMuonDz            {new TH1F("h_nonpromptMuonDz",       "nonprompt muon d_{z}; d_{z} (cm);", 500, 0., 1.)};
    TH1F* h_nonpromptMuonDxy           {new TH1F("h_nonpromptMuonDxy",      "nonprompt muon d_{xy}; d_{xy} (cm);", 500, 0., 1.)};
    TH1F* h_nonpromptMuonPdgId         {new TH1F("h_nonpromptMuonPdgId",    "nonprompt muon matched genPar Id", 501, -0.5, 500.5)};
    TH1F* h_nonpromptMuonMotherId      {new TH1F("h_nonpromptMuonMotherId", "nonprompt muon matched genPar mother Id", 502, -1.5, 500.5)};

    TH1F* h_recoDimuonMass      {new TH1F("h_recoDimuonMass", "Dimuon reco mass", 30, 0., 11.)};
    TH1F* h_recoDimuonPt        {new TH1F("h_recoDimuonPt",   "Dimuon reco Pt",  200, 0., 200)}; 
    TH1F* h_recoDimuonEta       {new TH1F("h_recoDimuonEta",  "Dimuon reco Eta", 60, 0., 5.)};

    TH2F* h_relIsoVsEta1        {new TH2F("h_relIsoVsEta1", "rel iso vs leading lepton #eta; #eta; rel iso" , 300, -3., 3., 100, 0., 1.0)};
    TH2F* h_relIsoVsEta2        {new TH2F("h_relIsoVsEta2", "rel iso vs subleading lepton #eta; #eta; rel iso" , 300, -3., 3., 100, 0., 1.0)};
    TH2F* h_dxyVsDz1            {new TH2F("h_dxyVsDz1", "dxy vs dz leading lepton; d_{xy} (cm); d_{z} (cm)", 500, 0., 1., 500, 0., 1.)};
    TH2F* h_dxyVsDz2            {new TH2F("h_dxyVsDz2", "dxy vs dz leading lepton; d_{xy} (cm); d_{z} (cm)", 500, 0., 1., 500, 0., 1.)};

    TProfile* p_leadingMuonsProfile     {new TProfile("p_leadingMuonsProfile", "", 3, 0.5, 3.5)};
    TProfile* p_subleadingMuonsProfile  {new TProfile("p_subleadingMuonsProfile", "", 3, 0.5, 3.5)};

    p_leadingMuonsProfile->GetXaxis()->SetBinLabel(1, "Scalar parentage");
    p_leadingMuonsProfile->GetXaxis()->SetBinLabel(2, "PromptFinalState");
    p_leadingMuonsProfile->GetXaxis()->SetBinLabel(3, "HardProcess");
    p_subleadingMuonsProfile->GetXaxis()->SetBinLabel(1, "Scalar parentage");
    p_subleadingMuonsProfile->GetXaxis()->SetBinLabel(2, "PromptFinalState");
    p_subleadingMuonsProfile->GetXaxis()->SetBinLabel(3, "HardProcess");

    // Reco Muon Vertex/Track Histos
    TProfile* p_muonTrackPairsAncestry   {new TProfile ("p_muonTrackPairsAncestry", "Number of genuine/combinatorial/fake muon pairs per event", 4, 0.5, 4.5, "ymax = 1.0")};
    p_muonTrackPairsAncestry->GetXaxis()->SetBinLabel(1, "Both muon tracks genuine");
    p_muonTrackPairsAncestry->GetXaxis()->SetBinLabel(2, "Leading muon track geunine");
    p_muonTrackPairsAncestry->GetXaxis()->SetBinLabel(3, "Subleading muon track genuine");
    p_muonTrackPairsAncestry->GetXaxis()->SetBinLabel(4, "Both genuine tracks fake");

    TProfile* p_selectedMuonTrkMatching  {new TProfile ("p_selectedMuonTrkMatching", "Selected leading/subleading muon ancestry", 4, 0.5, 4.5, "ymax = 1.0")};
    p_selectedMuonTrkMatching->GetXaxis()->SetBinLabel(1, "Both muon tracks genuine");
    p_selectedMuonTrkMatching->GetXaxis()->SetBinLabel(2, "Leading muon track geunine");
    p_selectedMuonTrkMatching->GetXaxis()->SetBinLabel(3, "Subleading muon track genuine");
    p_selectedMuonTrkMatching->GetXaxis()->SetBinLabel(4, "Both genuine tracks fake");

    TH1F* h_MuVtxPx               {new TH1F("h_MuVtxPx",          "Muon Vertex p_{x}; p_{x} (GeV);", 1000, -300., 300.)};
    TH1F* h_MuVtxPy               {new TH1F("h_MuVtxPy",          "Muon Vertex p_{y}; p_{y} (GeV);", 1000, -300., 300.)};
    TH1F* h_MuVtxPz               {new TH1F("h_MuVtxPz",          "Muon Vertex p_{z}; p_{z} (GeV);", 1000, -300., 300.)};
    TH1F* h_MuVtxP2               {new TH1F("h_MuVtxP2",          "Muon Vertex p^{2}; p^{2} (GeV)^{2};", 800, 0., 400.)};
    TH1F* h_MuTrkVx               {new TH1F("h_MuTrkVx",          "Muon Vertex v_{x}; v_{x} (cm)", 400, -100., 100.)}; 
    TH1F* h_MuTrkVy               {new TH1F("h_MuTrkVy",          "Muon Vertex v_{y}; v_{y} (cm)", 400, -100., 100.)}; 
    TH1F* h_MuTrkVz               {new TH1F("h_MuTrkVz",          "Muon Vertex v_{z}; v_{z} (cm)", 400, -100., 100.)}; 
    TH1F* h_MuTrkVabs             {new TH1F("h_MuTrkVabs",        "Muon Vertex v; v (cm)",  400, 0., 200.)}; 
    TH1F* h_MuVtxChi2Ndof         {new TH1F("h_MuVtxChi2Ndof",    "Muon Vertex #chi^{2}/N_{dof}; #chi^{2}/N_{dof}", 500, 0., 100.)};
    TH1F* h_MuVtxAngleXY          {new TH1F("h_MuVtxAngleXY",     "Muon Vertex 2D angle; 2D angle", 120, -1.5, 1.5)};
    TH1F* h_MuVtxAngleXYZ         {new TH1F("h_MuVtxAngleXYZ",    "Muon Vertex 3D angle; 3D angle", 120, -1.5, 1.5)};
    TH1F* h_MuVtxSigXY            {new TH1F("h_MuVtxSigXY",       "Muon Vertex XY Signifiance; XY Signifiance", 600, 0., 300.)};
    TH1F* h_MuVtxSigXYZ           {new TH1F("h_MuVtxSigXYZ",      "Muon Vertex XYZ Signifiance; XYZ Signifiance", 100, 0., 5.)};
    TH1F* h_MuTrkDca              {new TH1F("h_MuTrkDca",         "Muon vertex tracks' distance of closest approach; dca (cm)", 100, 0., 50.)};

    TH1F* h_genMuVtxPx            {new TH1F("h_genMuVtxPx",       "Genuine Muon Vertex p_{x}; p_{x} (GeV);", 1000, -300., 300.)};
    TH1F* h_genMuVtxPy            {new TH1F("h_genMuVtxPy",       "Genuine Muon Vertex p_{y}; p_{y} (GeV);", 1000, -300., 300.)};
    TH1F* h_genMuVtxPz            {new TH1F("h_genMuVtxPz",       "Genuine Muon Vertex p_{z}; p_{z} (GeV);", 1000, -300., 300.)};
    TH1F* h_genMuVtxP2            {new TH1F("h_genMuVtxP2",       "Genuine Muon Vertex p^{2}; p^{2} (GeV)^{2};", 800, 0., 400.)};
    TH1F* h_genMuTrkVx            {new TH1F("h_genMuTrkVx",       "Genuine Muon Vertex v_{x}; v_{x} (cm)", 400, -100., 100.)}; 
    TH1F* h_genMuTrkVy            {new TH1F("h_genMuTrkVy",       "Genuine Muon Vertex v_{y}; v_{y} (cm)", 400, -100., 100.)}; 
    TH1F* h_genMuTrkVz            {new TH1F("h_genMuTrkVz",       "Genuine Muon Vertex v_{z}; v_{z} (cm)", 400, -100., 100.)}; 
    TH1F* h_genMuTrkVabs          {new TH1F("h_genMuTrkVabs",     "Genuine Muon Vertex v; v (cm)",  400, 0., 200.)}; 
    TH1F* h_genMuVtxChi2Ndof      {new TH1F("h_genMuVtxChi2Ndof", "Genuine Muon Vertex #chi^{2}/N_{dof}; #chi^{2}/N_{dof}", 500, 0., 100.)}; 
    TH1F* h_genMuVtxAngleXY       {new TH1F("h_genMuVtxAngleXY",  "Genuine Muon Vertex 2D angle; 2D angle", 120, -1.5, 1.5)};
    TH1F* h_genMuVtxAngleXYZ      {new TH1F("h_genMuVtxAngleXYZ", "Genuine Muon Vertex 3D angle; 3D angle", 120, -1.5, 1.5)};
    TH1F* h_genMuVtxSigXY         {new TH1F("h_genMuVtxSigXY",    "Genuine Muon Vertex XY Signifiance; XY Signifiance", 600, 0., 300.)};
    TH1F* h_genMuVtxSigXYZ        {new TH1F("h_genMuVtxSigXYZ",   "Genuine Muon Vertex XYZ Signifiance; XYZ Signifiance", 100, 0., 5.)};
    TH1F* h_genMuTrkDca           {new TH1F("h_genMuTrkDca",      "Genuine Muon vertex tracks' distance of closest approach; dca (cm)", 100, 0., 50.)};

    TH1F* h_combMuVtxPx           {new TH1F("h_combMuVtxPx",      "Comb Muon Vertex p_{x}; p_{x} (GeV);", 1000, -300., 300.)};
    TH1F* h_combMuVtxPy           {new TH1F("h_combMuVtxPy",      "Comb Muon Vertex p_{y}; p_{y} (GeV);", 1000, -300., 300.)};
    TH1F* h_combMuVtxPz           {new TH1F("h_combMuVtxPz",      "Comb Muon Vertex p_{z}; p_{z} (GeV);", 1000, -300., 300.)};
    TH1F* h_combMuVtxP2           {new TH1F("h_combMuVtxP2",      "Comb Muon Vertex p^{2}; p^{2} (GeV)^{2};", 800, 0., 400.)};
    TH1F* h_combMuTrkVx           {new TH1F("h_combMuTrkVx",      "Comb Muon Vertex v_{x}; v_{x} (cm)", 400, -100., 100.)}; 
    TH1F* h_combMuTrkVy           {new TH1F("h_combMuTrkVy",      "Comb Muon Vertex v_{y}; v_{y} (cm)", 400, -100., 100.)}; 
    TH1F* h_combMuTrkVz           {new TH1F("h_combMuTrkVz",      "Comb Muon Vertex v_{z}; v_{z} (cm)", 400, -100., 100.)}; 
    TH1F* h_combMuTrkVabs         {new TH1F("h_combMuTrkVabs",    "Comb Muon Vertex v; v (cm)",  400, 0., 200.)}; 
    TH1F* h_combMuVtxChi2Ndof     {new TH1F("h_combMuVtxChi2Ndof","Comb Muon Vertex #chi^{2}/N_{dof}; #chi^{2}/N_{dof}", 500, 0., 100.)}; 
    TH1F* h_combMuVtxAngleXY      {new TH1F("h_combMuVtxAngleXY", "Comb Muon Vertex 2D angle; 2D angle", 120, -1.5, 1.5)};
    TH1F* h_combMuVtxAngleXYZ     {new TH1F("h_combMuVtxAngleXYZ","Comb Muon Vertex 3D angle; 3D angle", 120, -1.5, 1.5)};
    TH1F* h_combMuVtxSigXY        {new TH1F("h_combMuVtxSigXY",   "Comb Muon Vertex XY Signifiance; XY Signifiance", 600, 0., 300.)};
    TH1F* h_combMuVtxSigXYZ       {new TH1F("h_combMuVtxSigXYZ",  "Comb Muon Vertex XYZ Signifiance; XYZ Signifiance", 100, 0., 5.)};
    TH1F* h_combMuTrkDca          {new TH1F("h_combMuTrkDca",     "Comb Muon vertex tracks' distance of closest approach; dca (cm)", 100, 0., 50.)};

    TH1F* h_fakeMuVtxPx           {new TH1F("h_fakeMuVtxPx",      "Fake Muon Vertex p_{x}; p_{x} (GeV);", 1000, -300., 300.)};
    TH1F* h_fakeMuVtxPy           {new TH1F("h_fakeMuVtxPy",      "Fake Muon Vertex p_{y}; p_{y} (GeV);", 1000, -300., 300.)};
    TH1F* h_fakeMuVtxPz           {new TH1F("h_fakeMuVtxPz",      "Fake Muon Vertex p_{z}; p_{z} (GeV);", 1000, -300., 300.)};
    TH1F* h_fakeMuVtxP2           {new TH1F("h_fakeMuVtxP2",      "Fake Muon Vertex p^{2}; p^{2} (GeV)^{2};", 800, 0., 400.)};
    TH1F* h_fakeMuTrkVx           {new TH1F("h_fakeMuTrkVx",      "Fake Muon Vertex v_{x}; v_{x} (cm)", 400, -100., 100.)}; 
    TH1F* h_fakeMuTrkVy           {new TH1F("h_fakeMuTrkVy",      "Fake Muon Vertex v_{y}; v_{y} (cm)", 400, -100., 100.)}; 
    TH1F* h_fakeMuTrkVz           {new TH1F("h_fakeMuTrkVz",      "Fake Muon Vertex v_{z}; v_{z} (cm)", 400, -100., 100.)}; 
    TH1F* h_fakeMuTrkVabs         {new TH1F("h_fakeMuTrkVabs",    "Fake Muon Vertex v; v (cm)",  400, 0., 200.)}; 
    TH1F* h_fakeMuVtxChi2Ndof     {new TH1F("h_fakeMuVtxChi2Ndof","Fake Muon Vertex #chi^{2}/N_{dof}; #chi^{2}/N_{dof}", 500, 0., 100.)}; 
    TH1F* h_fakeMuVtxAngleXY      {new TH1F("h_fakeMuVtxAngleXY", "Fake Muon Vertex 2D angle; 2D angle", 120, -1.5, 1.5)};
    TH1F* h_fakeMuVtxAngleXYZ     {new TH1F("h_fakeMuVtxAngleXYZ","Fake Muon Vertex 3D angle; 3D angle", 120, -1.5, 1.5)};
    TH1F* h_fakeMuVtxSigXY        {new TH1F("h_fakeMuVtxSigXY",   "Fake Muon Vertex XY Signifiance; XY Signifiance", 600, 0., 300.)};
    TH1F* h_fakeMuVtxSigXYZ       {new TH1F("h_fakeMuVtxSigXYZ",  "Fake Muon Vertex XYZ Signifiance; XYZ Signifiance", 100, 0., 5.)};
    TH1F* h_fakeMuTrkDca          {new TH1F("h_fakeMuTrkDca",     "Fake Muon vertex tracks' distance of closest approach; dca (cm)", 100, 0., 50.)};

    // Reco Muon Vertex/Refitted Track Histos
    TH1F* h_originalGenMuTrkPt1         {new TH1F("h_originalGenMuTrkPt1",        "Pre-refit track p_{T} (genuine)", 400, 0., 200.)};
    TH1F* h_originalGenMuTrkEta1        {new TH1F("h_originalGenMuTrkEta1",       "Pre-refit track #eta (genuine)", 600, -3., 3.)};
    TH1F* h_originalGenMuTrkChi2Ndof1   {new TH1F("h_originalGenMuTrkChi2Ndof1",  "Pre-refit track #chi^{2}/N_{dof} (genuine)", 500, 0., 100.)}; 
    TH1F* h_refittedGenMuTrkPt1         {new TH1F("h_refittedGenMuTrkPt1",        "Refitted track p_{T} (genuine)", 400, 0., 200.)};
    TH1F* h_refittedGenMuTrkEta1        {new TH1F("h_refittedGenMuTrkEta1",       "Refitted track #eta (genuine)", 600, -3., 3.)};
    TH1F* h_refittedGenMuTrkChi2Ndof1   {new TH1F("h_refittedGenMuTrkChi2Ndof1",  "Refitted track #chi^{2}/N_{dof} (genuine)", 500, 0., 100.)};

    TH1F* h_originalFakeMuTrkPt1        {new TH1F("h_originalFakeMuTrkPt1",        "Pre-refit track p_{T} (fake)", 400, 0., 200.)};
    TH1F* h_originalFakeMuTrkEta1       {new TH1F("h_originalFakeMuTrkEta1",       "Pre-refit track #eta (fake)", 600, -3., 3.)};
    TH1F* h_originalFakeMuTrkChi2Ndof1  {new TH1F("h_originalFakeMuTrkChi2Ndof1",  "Pre-refit track #chi^{2}/N_{dof} (fake)", 500, 0., 100.)}; 
    TH1F* h_refittedFakeMuTrkPt1        {new TH1F("h_refittedFakeMuTrkPt1",        "Re-refitted track p_{T} (fake)", 400, 0., 200.)};
    TH1F* h_refittedFakeMuTrkEta1       {new TH1F("h_refittedFakeMuTrkEta1",       "Re-refitted track #eta (fake)", 600, -3., 3.)};
    TH1F* h_refittedFakeMuTrkChi2Ndof1  {new TH1F("h_refittedFakeMuTrkChi2Ndof1",  "Refitted track #chi^{2}/N_{dof} (fake)", 500, 0., 100.)};

    TH1F* h_originalGenMuTrkPt2         {new TH1F("h_originalGenMuTrkPt2",        "Pre-refit track p_{T} (genuine)", 400, 0., 200.)};
    TH1F* h_originalGenMuTrkEta2        {new TH1F("h_originalGenMuTrkEta2",       "Pre-refit track #eta (genuine)", 600, -3., 3.)};
    TH1F* h_originalGenMuTrkChi2Ndof2   {new TH1F("h_originalGenMuTrkChi2Ndof2",  "Pre-refit track #chi^{2}/N_{dof} (genuine)", 500, 0., 100.)}; 
    TH1F* h_refittedGenMuTrkPt2         {new TH1F("h_refittedGenMuTrkPt2",        "Refitted track p_{T} (genuine)", 400, 0., 200.)};
    TH1F* h_refittedGenMuTrkEta2        {new TH1F("h_refittedGenMuTrkEta2",       "Refitted track #eta (genuine)", 600, -3., 3.)};
    TH1F* h_refittedGenMuTrkChi2Ndof2   {new TH1F("h_refittedGenMuTrkChi2Ndof2",  "Refitted track #chi^{2}/N_{dof} (genuine)", 500, 0., 100.)};

    TH1F* h_originalFakeMuTrkPt2        {new TH1F("h_originalFakeMuTrkPt2",        "Pre-refit track p_{T} (fake)", 400, 0., 200.)};
    TH1F* h_originalFakeMuTrkEta2       {new TH1F("h_originalFakeMuTrkEta2",       "Pre-refit track #eta (fake)", 600, -3., 3.)};
    TH1F* h_originalFakeMuTrkChi2Ndof2  {new TH1F("h_originalFakeMuTrkChi2Ndof2",  "Pre-refit track #chi^{2}/N_{dof} (fake)", 500, 0., 100.)}; 
    TH1F* h_refittedFakeMuTrkPt2        {new TH1F("h_refittedFakeMuTrkPt2",        "Re-refitted track p_{T} (fake)", 400, 0., 200.)};
    TH1F* h_refittedFakeMuTrkEta2       {new TH1F("h_refittedFakeMuTrkEta2",       "Re-refitted track #eta (fake)", 600, -3., 3.)};
    TH1F* h_refittedFakeMuTrkChi2Ndof2  {new TH1F("h_refittedFakeMuTrkChi2Ndof2",  "Refitted track #chi^{2}/N_{dof} (fake)", 500, 0., 100.)};

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
        "2016", po::bool_switch(&is2016_), "Use 2016 conditions (SFs, et al.).")(
        "scalarMass,s",
        po::value<float>(&scalarMass_)->default_value(2.0),
        "scalar mass being searched for.")(
        ",e", po::bool_switch(&exactlyTwo_), "only use events with exactly two loose muons");
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

            // gen particle loop
            for ( Int_t k{0}; k < event.nGenPar; k++ ) {
                const int pid { std::abs(event.genParId[k]) };
                if (pid != 321) continue;

                const bool isScalarGrandparent {scalarGrandparent(event,k,9000006)}; 
                if ( !isScalarGrandparent ) continue;

                const bool isChargedDmesonGrandparent {scalarGrandparent(event,k, 411)};
                const bool isNeutralDmesonGrandparent {scalarGrandparent(event,k, 421)};
                const bool isChargedDshortGrandparent {scalarGrandparent(event,k, 431)};

                p_kaonAncestry->Fill(1.0, (!isChargedDmesonGrandparent && !isNeutralDmesonGrandparent && !isChargedDshortGrandparent) );
                p_kaonAncestry->Fill(2.0, (isChargedDmesonGrandparent || isNeutralDmesonGrandparent || isChargedDshortGrandparent) );
                p_kaonAncestry->Fill(3.0,isChargedDmesonGrandparent);
                p_kaonAncestry->Fill(4.0,isNeutralDmesonGrandparent);
                p_kaonAncestry->Fill(5.0,isChargedDshortGrandparent);
          
            }

            float eventWeight = 1.;

            if (!event.metFilters()) continue;

            const bool passSingleMuonTrigger {event.muTrig()}, passDimuonTrigger {event.mumuTrig()};
            const bool passL2MuonTrigger {event.mumuL2Trig()}, passDimuonNoVtxTrigger {event.mumuNoVtxTrig()};

            if ( event.numMuonPF2PAT > 1 ) {
                // fill muon pT plots pre-triggers
                //// ID requirements PF muon? no pT cut
                //// reco pT 
                int mu1 {-1}, mu2{-1};
                for ( Int_t k{0}; k < event.numMuonPF2PAT; k++ ) {
                    if ( event.genMuonPF2PATMotherId[k] == 9000006 && mu1 < 0 ) mu1 = k;
                    else if ( event.genMuonPF2PATMotherId[k] == 9000006 && mu2 < 0 ) mu2 = k;
                    else if (mu1 >= 0 && mu2 > 0) break;
                }

                float delR_truth = deltaR(event.muonPF2PATEta[mu1], event.muonPF2PATPhi[mu1], event.muonPF2PATEta[mu2], event.muonPF2PATPhi[mu2]);
                float delR = deltaR(event.muonPF2PATEta[0], event.muonPF2PATPhi[0], event.muonPF2PATEta[1], event.muonPF2PATPhi[1]);
                // Fill general pT/dR (with and without scalar parentage)
                h_leadingMuonPt_truth->Fill(event.muonPF2PATPt[mu1]);
                h_subLeadingMuonPt_truth->Fill(event.muonPF2PATPt[mu2]);
                h_leadingMuonPt->Fill(event.muonPF2PATPt[0]);
                h_subLeadingMuonPt->Fill(event.muonPF2PATPt[1]);
                h_delR_truth->Fill(delR_truth);
                h_delR->Fill(delR);
                // Fill pT post trigger (with and without scalar parentage)
                if (passSingleMuonTrigger) {
                    h_leadingMuonPt_truth_muTrig->Fill(event.muonPF2PATPt[mu1]);
                    h_subLeadingMuonPt_truth_muTrig->Fill(event.muonPF2PATPt[mu2]);
                    h_leadingMuonPt_muTrig->Fill(event.muonPF2PATPt[0]);
                    h_subLeadingMuonPt_muTrig->Fill(event.muonPF2PATPt[1]);
                    h_delR_truth_muTrig->Fill(delR_truth);
                    h_delR_muTrig->Fill(delR);
                }
                if (passDimuonTrigger) {
                    h_leadingMuonPt_truth_mumuTrig->Fill(event.muonPF2PATPt[mu1]);
                    h_subLeadingMuonPt_truth_mumuTrig->Fill(event.muonPF2PATPt[mu2]);
                    h_leadingMuonPt_mumuTrig->Fill(event.muonPF2PATPt[0]);
                    h_subLeadingMuonPt_mumuTrig->Fill(event.muonPF2PATPt[1]);
                    h_delR_truth_mumuTrig->Fill(delR_truth);
                    h_delR_mumuTrig->Fill(delR);
                }
                if (passL2MuonTrigger || passDimuonNoVtxTrigger) {
                    h_leadingMuonPt_truth_L2muTrig->Fill(event.muonPF2PATPt[mu1]);
                    h_subLeadingMuonPt_truth_L2muTrig->Fill(event.muonPF2PATPt[mu2]);
                    h_leadingMuonPt_L2muTrig->Fill(event.muonPF2PATPt[0]);
                    h_subLeadingMuonPt_L2muTrig->Fill(event.muonPF2PATPt[1]);
                    h_delR_truth_L2muTrig->Fill(delR_truth);
                    h_delR_L2muTrig->Fill(delR);
                }
            }

//            if (! ( passDimuonTrigger || passSingleMuonTrigger ) ) continue;

            std::vector<int> looseMuonIndex = getLooseMuons(event);

            h_numMuons->Fill(looseMuonIndex.size());

            std::vector<int> promptLooseMuonIndex     = getPromptMuons(event, looseMuonIndex, true);
            std::vector<int> nonpromptLooseMuonIndex  = getPromptMuons(event, looseMuonIndex, false);

            int numMuonsFromScalar{0}, numMuonsPromptFinalState{0}, numMuonsHardProcess{0};

            // reco muon loop
            for ( auto it = looseMuonIndex.begin(); it != looseMuonIndex.end(); it++ ) {
      	       	if (event.genMuonPF2PATMotherId[*it] == 9000006) numMuonsFromScalar++;
                if (event.genMuonPF2PATPromptFinalState[*it]) numMuonsPromptFinalState++;

                int motherId = std::abs(event.genMuonPF2PATMotherId[*it]);
                if (motherId == 9000006) motherId = -1.;

      	       	if (event.genMuonPF2PATHardProcess[*it]) {
                    numMuonsHardProcess++;

                    p_hardMuonsIdProfile->Fill(1.0, event.muonPF2PATIsPFMuon[*it]);
                    p_hardMuonsIdProfile->Fill(2.0, event.muonPF2PATLooseCutId[*it]);
                    p_hardMuonsIdProfile->Fill(3.0, event.muonPF2PATMediumCutId[*it]);
                    p_hardMuonsIdProfile->Fill(4.0, event.muonPF2PATTightCutId[*it]);
                    p_hardMuonsIdProfile->Fill(5.0, event.muonPF2PATPfIsoVeryLoose[*it]);
                    p_hardMuonsIdProfile->Fill(6.0, event.muonPF2PATPfIsoLoose[*it]);
                    p_hardMuonsIdProfile->Fill(7.0, event.muonPF2PATPfIsoMedium[*it]);
                    p_hardMuonsIdProfile->Fill(8.0, event.muonPF2PATPfIsoTight[*it]);
                    p_hardMuonsIdProfile->Fill(9.0, event.muonPF2PATPfIsoVeryTight[*it]);

                    h_promptMuonPt->Fill(event.muonPF2PATPt[*it]);
                    h_promptMuonEta->Fill(std::abs(event.muonPF2PATEta[*it]));
                    h_promptMuonPhi->Fill(event.muonPF2PATPhi[*it]);
                    h_promptMuonRelIso->Fill(event.muonPF2PATComRelIsodBeta[*it]);
                    h_promptMuonDz->Fill(event.muonPF2PATDZPV[*it]);
                    h_promptMuonDxy->Fill(event.muonPF2PATDBPV[*it]);
                    h_promptMuonPdgId->Fill(std::abs(event.genMuonPF2PATPdgId[*it]));
                    h_promptMuonMotherId->Fill(motherId);
                }
                else {
                    p_softMuonsIdProfile->Fill(1.0, event.muonPF2PATIsPFMuon[*it]);
                    p_softMuonsIdProfile->Fill(2.0, event.muonPF2PATLooseCutId[*it]);
                    p_softMuonsIdProfile->Fill(3.0, event.muonPF2PATMediumCutId[*it]);
                    p_softMuonsIdProfile->Fill(4.0, event.muonPF2PATTightCutId[*it]);
                    p_softMuonsIdProfile->Fill(5.0, event.muonPF2PATPfIsoVeryLoose[*it]);
                    p_softMuonsIdProfile->Fill(6.0, event.muonPF2PATPfIsoLoose[*it]);
                    p_softMuonsIdProfile->Fill(7.0, event.muonPF2PATPfIsoMedium[*it]);
                    p_softMuonsIdProfile->Fill(8.0, event.muonPF2PATPfIsoTight[*it]);
                    p_softMuonsIdProfile->Fill(9.0, event.muonPF2PATPfIsoVeryTight[*it]);

                    h_nonpromptMuonPt->Fill(event.muonPF2PATPt[*it]);
                    h_nonpromptMuonEta->Fill(std::abs(event.muonPF2PATEta[*it]));
                    h_nonpromptMuonPhi->Fill(event.muonPF2PATPhi[*it]);
                    h_nonpromptMuonRelIso->Fill(event.muonPF2PATComRelIsodBeta[*it]);
                    h_nonpromptMuonDz->Fill(event.muonPF2PATDZPV[*it]);
                    h_nonpromptMuonDxy->Fill(event.muonPF2PATDBPV[*it]);
                    h_nonpromptMuonPdgId->Fill(std::abs(event.genMuonPF2PATPdgId[*it]));
                    h_nonpromptMuonMotherId->Fill(motherId);
                }
            } // end reco muon loop

            h_numMuonsFromScalar->Fill(numMuonsFromScalar);
            h_numMuonsPromptFinalState->Fill(numMuonsPromptFinalState);
            h_numMuonsHardProcess->Fill(numMuonsHardProcess);
            

            if ( looseMuonIndex.size() != 2 ) continue;

            if ( !getDileptonCand( event, looseMuonIndex, scalarMass_, exactlyTwo_) ) continue;

            if ( (event.zPairLeptons.first + event.zPairLeptons.second).M() > invZMassCut_ ) continue;

            eventWeight *= datasetWeight;

            const int index1 {event.zPairIndex.first}, index2 {event.zPairIndex.second};

            h_recoDimuonMass->Fill( (event.zPairLeptons.first+event.zPairLeptons.second).M() );
            h_recoDimuonPt->Fill( (event.zPairLeptons.first+event.zPairLeptons.second).Pt() );
            h_recoDimuonEta->Fill( (event.zPairLeptons.first+event.zPairLeptons.second).Eta() );

            h_relIsoVsEta1->Fill( event.zPairLeptons.first.Eta(),  event.zPairRelIso.first, eventWeight );
            h_relIsoVsEta2->Fill( event.zPairLeptons.second.Eta(), event.zPairRelIso.second, eventWeight );

            h_dxyVsDz1->Fill ( event.muonPF2PATDZPV[index1], event.muonPF2PATDBPV[index1], eventWeight );
            h_dxyVsDz2->Fill ( event.muonPF2PATDZPV[index2], event.muonPF2PATDBPV[index2], eventWeight );

            const int genMuonFromScalar1 { event.genMuonPF2PATMotherId[index1] == 9000006 ? 1 : 0 };
            const int genMuonFromScalar2 { event.genMuonPF2PATMotherId[index2] == 9000006 ? 1 : 0 };

            p_leadingMuonsProfile->Fill( 1.0, genMuonFromScalar1 );
      	    p_leadingMuonsProfile->Fill( 2.0, event.genMuonPF2PATPromptFinalState[index1] );
      	    p_leadingMuonsProfile->Fill( 3.0, event.genMuonPF2PATHardProcess[index1] );

            p_subleadingMuonsProfile->Fill( 1.0, genMuonFromScalar2 );
      	    p_subleadingMuonsProfile->Fill( 2.0, event.genMuonPF2PATPromptFinalState[index2] );
      	    p_subleadingMuonsProfile->Fill( 3.0, event.genMuonPF2PATHardProcess[index2] );

            unsigned int numGenuineMuonTracks{0}, numComb1MuonTracks{0}, numComb2MuonTracks{0}, numFakeMuonTracks{0};

            // loop over muon tracks post lepton selection
            for ( Int_t k{0}; k < event.numMuonTrackPairsPF2PAT; k++ ) {

                // Number of pairs of muon tracks per event where both muons came from scalar
                // Number of pairs of muon tracks per event where only one muon came from scalar
                // Number of pairs of muon tracks per event where only no muons came from scalar

                const int index1{event.muonTkPairPF2PATIndex1[k]}, index2{event.muonTkPairPF2PATIndex2[k]};
                const int muonMotherId1{event.genMuonPF2PATMotherId[index1]}, muonMotherId2{event.genMuonPF2PATMotherId[index2]};

                const bool genMuon1 {muonMotherId1 == 9000006}, genMuon2 {muonMotherId2 == 9000006};

                if ( genMuon1 && genMuon2 )      numGenuineMuonTracks++;
                else if ( genMuon1 && !genMuon2 ) numComb1MuonTracks++;
                else if ( !genMuon1 && genMuon2 ) numComb2MuonTracks++;
                else if ( !genMuon1 && !genMuon2 ) numFakeMuonTracks++;

                // Are leading and subleading muons selected from scalar?
                if ( index1 == event.zPairIndex.first && index2 == event.zPairIndex.second ) {
                    p_selectedMuonTrkMatching->Fill( 1.0, bool (genMuon1 && genMuon2) );
                    p_selectedMuonTrkMatching->Fill( 2.0, bool (genMuon1 && !genMuon2) );
                    p_selectedMuonTrkMatching->Fill( 3.0, bool (!genMuon1 && genMuon2) );
                    p_selectedMuonTrkMatching->Fill( 4.0, bool (!genMuon1 && !genMuon2) );
                }


                const float vtxPx {event.muonTkPairPF2PATTkVtxPx[k]}, vtxPy {event.muonTkPairPF2PATTkVtxPy[k]}, vtxPz {event.muonTkPairPF2PATTkVtxPz[k]}, vtxP2 {std::sqrt(event.muonTkPairPF2PATTkVtxP2[k])};
                const float vx {event.muonTkPairPF2PATTkVx[k]}, vy {event.muonTkPairPF2PATTkVy[k]}, vz {event.muonTkPairPF2PATTkVz[k]};
                const float Vabs {std::sqrt( vx*vx + vy*vy + vz*vz )};
       	       	const float vtxChi2Ndof {event.muonTkPairPF2PATTkVtxChi2[k]/(event.muonTkPairPF2PATTkVtxNdof[k]+1.0e-06)};
                const float angleXY {event.muonTkPairPF2PATTkVtxAngleXY[k]}, angleXYZ {event.muonTkPairPF2PATTkVtxAngleXYZ[k]};
                const float sigXY {event.muonTkPairPF2PATTkVtxDistMagXY[k]/(event.muonTkPairPF2PATTkVtxDistMagXYSigma[k]+1.0e-06)}, sigXYZ {event.muonTkPairPF2PATTkVtxDistMagXYZ[k]/(event.muonTkPairPF2PATTkVtxDistMagXYZ[k]+1.0e-06)};
                const float dca {event.muonTkPairPF2PATTkVtxDcaPreFit[k]};

                // Plot all muon vertex quantities
                h_MuVtxPx->Fill(vtxPx);
                h_MuVtxPy->Fill(vtxPy);
                h_MuVtxPz->Fill(vtxPz);
                h_MuVtxP2->Fill(vtxP2);
                h_MuTrkVx->Fill(vx);
                h_MuTrkVy->Fill(vy);
                h_MuTrkVz->Fill(vz);
                h_MuTrkVabs->Fill(Vabs);
                h_MuVtxChi2Ndof->Fill(vtxChi2Ndof);
                h_MuVtxAngleXY->Fill(angleXY);
                h_MuVtxAngleXYZ->Fill(angleXYZ);
                h_MuVtxSigXY->Fill(sigXY);
                h_MuVtxSigXYZ->Fill(sigXYZ);
                h_MuTrkDca->Fill(dca);

                // Plot genuine muon vetrex quantities
                if ( genMuon1 && genMuon2 ) {
                    h_genMuVtxPx->Fill(vtxPx);
                    h_genMuVtxPy->Fill(vtxPy);
                    h_genMuVtxPz->Fill(vtxPz);
                    h_genMuVtxP2->Fill(vtxP2);
                    h_genMuTrkVx->Fill(vx);
                    h_genMuTrkVy->Fill(vy);
                    h_genMuTrkVz->Fill(vz);
                    h_genMuTrkVabs->Fill(Vabs);
                    h_genMuVtxChi2Ndof->Fill(vtxChi2Ndof);
                    h_genMuVtxAngleXY->Fill(angleXY);
                    h_genMuVtxAngleXYZ->Fill(angleXYZ);
                    h_genMuVtxSigXY->Fill(sigXY);
                    h_genMuVtxSigXYZ->Fill(sigXYZ);
                    h_genMuTrkDca->Fill(dca);
                }
                else if ( (genMuon1 && !genMuon2) || (!genMuon1 && genMuon2) ) {
                    h_combMuVtxPx->Fill(vtxPx);
                    h_combMuVtxPy->Fill(vtxPy);
                    h_combMuVtxPz->Fill(vtxPz);
                    h_combMuVtxP2->Fill(vtxP2);
                    h_combMuTrkVx->Fill(vx);
                    h_combMuTrkVy->Fill(vy);
                    h_combMuTrkVz->Fill(vz);
                    h_combMuTrkVabs->Fill(Vabs);
                    h_combMuVtxChi2Ndof->Fill(vtxChi2Ndof);
                    h_combMuVtxAngleXY->Fill(angleXY);
                    h_combMuVtxAngleXYZ->Fill(angleXYZ);
                    h_combMuVtxSigXY->Fill(sigXY);
                    h_combMuVtxSigXYZ->Fill(sigXYZ);
                    h_combMuTrkDca->Fill(dca);
                }
                else if ( !genMuon2 && !genMuon2 ) {
                    h_fakeMuVtxPx->Fill(vtxPx);
                    h_fakeMuVtxPy->Fill(vtxPy);
                    h_fakeMuVtxPz->Fill(vtxPz);
                    h_fakeMuVtxP2->Fill(vtxP2);
                    h_fakeMuTrkVx->Fill(vx);
                    h_fakeMuTrkVy->Fill(vy);
                    h_fakeMuTrkVz->Fill(vz);
                    h_fakeMuTrkVabs->Fill(Vabs);
                    h_fakeMuVtxChi2Ndof->Fill(vtxChi2Ndof);
                    h_fakeMuVtxAngleXY->Fill(angleXY);
                    h_fakeMuVtxAngleXYZ->Fill(angleXYZ);
                    h_fakeMuVtxSigXY->Fill(sigXY);
                    h_fakeMuVtxSigXYZ->Fill(sigXYZ);
                    h_fakeMuTrkDca->Fill(dca);
                }


                const float oldTrkPt1 {event.muonPF2PATInnerTkPt[index1]}, oldTrkPt2 {event.muonPF2PATInnerTkPt[index2]};
                const float oldTrkEta1 {event.muonPF2PATInnerTkEta[index1]}, oldTrkEta2 {event.muonPF2PATInnerTkEta[index2]};
                const float oldTrkChi2Ndof1 {event.muonPF2PATInnerTkNormChi2[index1]}, oldTrkChi2Ndof2 {event.muonPF2PATInnerTkNormChi2[index2]};
                const float newTrkPt1 {event.muonTkPairPF2PATTk1Pt[k]}, newTrkPt2 {event.muonTkPairPF2PATTk2Pt[k]};
                const float newTrkEta1 {event.muonTkPairPF2PATTk1Eta[k]}, newTrkEta2 {event.muonTkPairPF2PATTk2Eta[k]};
                const float newTrkChi2Ndof1 {event.muonTkPairPF2PATTk1Chi2[k]/(event.muonTkPairPF2PATTk1Ndof[k]+1.0e-06)}, newTrkChi2Ndof2 {event.muonTkPairPF2PATTk2Chi2[k]/(event.muonTkPairPF2PATTk2Ndof[k]+1.0e-06)};

                // Plot replotted muon track pT, eta, chi2/ndof and dca prefit for genuine and fake muons
                if ( genMuon1 ) {
                    h_originalGenMuTrkPt1->Fill(oldTrkPt1);
                    h_originalGenMuTrkEta1->Fill(oldTrkEta1);
                    h_originalGenMuTrkChi2Ndof1->Fill(oldTrkChi2Ndof1);
                    h_refittedGenMuTrkPt1->Fill(newTrkPt1);
                    h_refittedGenMuTrkEta1->Fill(newTrkEta1);
                    h_refittedGenMuTrkChi2Ndof1->Fill(newTrkChi2Ndof1);
                }
                else {
                    h_originalFakeMuTrkPt1->Fill(oldTrkPt1);
                    h_originalFakeMuTrkEta1->Fill(oldTrkEta1);
                    h_originalFakeMuTrkChi2Ndof1->Fill(oldTrkChi2Ndof1);
                    h_refittedFakeMuTrkPt1->Fill(newTrkPt1);
                    h_refittedFakeMuTrkEta1->Fill(newTrkEta1);
                    h_refittedFakeMuTrkChi2Ndof1->Fill(newTrkChi2Ndof1);
                }
                if ( genMuon2 ) {
                    h_originalGenMuTrkPt2->Fill(oldTrkPt2);
                    h_originalGenMuTrkEta2->Fill(oldTrkEta2);
                    h_originalGenMuTrkChi2Ndof2->Fill(oldTrkChi2Ndof2);
                    h_refittedGenMuTrkPt2->Fill(newTrkPt2);
                    h_refittedGenMuTrkEta2->Fill(newTrkEta2);
                    h_refittedGenMuTrkChi2Ndof2->Fill(newTrkChi2Ndof2);
                }
                else {
                    h_originalFakeMuTrkPt2->Fill(oldTrkPt2);
                    h_originalFakeMuTrkEta2->Fill(oldTrkEta2);
                    h_originalFakeMuTrkPt2->Fill(oldTrkChi2Ndof2);
                    h_refittedFakeMuTrkChi2Ndof2->Fill(newTrkPt2);
                    h_refittedFakeMuTrkEta2->Fill(newTrkEta2);
                    h_refittedFakeMuTrkChi2Ndof2->Fill(newTrkChi2Ndof2);
                }
            }

            p_muonTrackPairsAncestry->Fill( 1.0, numGenuineMuonTracks );
            p_muonTrackPairsAncestry->Fill( 2.0, numComb1MuonTracks );
            p_muonTrackPairsAncestry->Fill( 3.0, numComb2MuonTracks );
            p_muonTrackPairsAncestry->Fill( 4.0, numFakeMuonTracks );

        } // end event loop
    } // end dataset loop

    TFile* outFile{new TFile{outFileString.c_str(), "RECREATE"}};
    outFile->cd();

    p_kaonAncestry->Write();

    h_leadingMuonPt_truth_muTrig->Divide(h_leadingMuonPt_truth);
    h_subLeadingMuonPt_truth_muTrig->Divide(h_subLeadingMuonPt_truth);
    h_leadingMuonPt_muTrig->Divide(h_leadingMuonPt);
    h_subLeadingMuonPt_muTrig->Divide(h_subLeadingMuonPt);
    h_delR_truth_muTrig->Divide(h_delR_truth);
    h_delR_muTrig->Divide(h_delR);

    h_leadingMuonPt_truth_mumuTrig->Divide(h_leadingMuonPt_truth);
    h_subLeadingMuonPt_truth_mumuTrig->Divide(h_subLeadingMuonPt_truth);
    h_leadingMuonPt_mumuTrig->Divide(h_leadingMuonPt);
    h_subLeadingMuonPt_mumuTrig->Divide(h_subLeadingMuonPt);
    h_delR_truth_mumuTrig->Divide(h_delR_truth);
    h_delR_mumuTrig->Divide(h_delR);

    h_leadingMuonPt_truth_L2muTrig->Divide(h_leadingMuonPt_truth);
    h_subLeadingMuonPt_truth_L2muTrig->Divide(h_subLeadingMuonPt_truth);
    h_leadingMuonPt_L2muTrig->Divide(h_leadingMuonPt);
    h_subLeadingMuonPt_L2muTrig->Divide(h_subLeadingMuonPt);
    h_delR_truth_L2muTrig->Divide(h_delR_truth);
    h_delR_L2muTrig->Divide(h_delR);

    h_leadingMuonPt_truth_muTrig->Write();
    h_subLeadingMuonPt_truth_muTrig->Write();
    h_leadingMuonPt_muTrig->Write();
    h_subLeadingMuonPt_muTrig->Write();
    h_delR_truth_muTrig->Write();
    h_delR_muTrig->Write();
    h_leadingMuonPt_truth_mumuTrig->Write();
    h_subLeadingMuonPt_truth_mumuTrig->Write();
    h_leadingMuonPt_mumuTrig->Write();
    h_subLeadingMuonPt_mumuTrig->Write();
    h_delR_truth_mumuTrig->Write();
    h_delR_mumuTrig->Write();
    h_leadingMuonPt_truth_L2muTrig->Write();
    h_subLeadingMuonPt_truth_L2muTrig->Write();
    h_leadingMuonPt_L2muTrig->Write();
    h_subLeadingMuonPt_L2muTrig->Write();
    h_delR_truth_L2muTrig->Write();
    h_delR_L2muTrig->Write();


    h_numMuons->Write();
    h_numMuonsFromScalar->Write();
    h_numMuonsPromptFinalState->Write();
    h_numMuonsHardProcess->Write();

    p_hardMuonsIdProfile->Write();
    p_softMuonsIdProfile->Write();

    h_promptMuonPt->Write();
    h_promptMuonEta->Write();
    h_promptMuonPhi->Write();
    h_promptMuonRelIso->Write();
    h_promptMuonDz->Write();
    h_promptMuonDxy->Write();
    h_promptMuonPdgId->Write();
    h_promptMuonMotherId->Write();
    h_nonpromptMuonPt->Write();
    h_nonpromptMuonEta->Write();
    h_nonpromptMuonPhi->Write();
    h_nonpromptMuonRelIso->Write();
    h_nonpromptMuonDz->Write();
    h_nonpromptMuonDxy->Write();
    h_nonpromptMuonPdgId->Write();
    h_nonpromptMuonMotherId->Write();


    h_recoDimuonMass->Write();
    h_recoDimuonPt->Write();
    h_recoDimuonEta->Write();

    h_relIsoVsEta1->Write();
    h_relIsoVsEta2->Write();
    h_dxyVsDz1->Write();
    h_dxyVsDz2->Write();

    p_leadingMuonsProfile->Write();
    p_subleadingMuonsProfile->Write();

    // muon vertex/track plots
    p_muonTrackPairsAncestry->Write();
    p_selectedMuonTrkMatching->Write();

    h_MuVtxPx->Write();
    h_MuVtxPy->Write();
    h_MuVtxPz->Write();
    h_MuVtxP2->Write();
    h_MuTrkVx->Write();
    h_MuTrkVy->Write();
    h_MuTrkVz->Write();
    h_MuTrkVabs->Write();
    h_MuVtxChi2Ndof->Write();
    h_MuVtxAngleXY->Write();
    h_MuVtxAngleXYZ->Write();
    h_MuVtxSigXY->Write();
    h_MuVtxSigXYZ->Write();
    h_MuTrkDca->Write();

    h_genMuVtxPx->Write();
    h_genMuVtxPy->Write();
    h_genMuVtxPz->Write();
    h_genMuVtxP2->Write();
    h_genMuTrkVx->Write();
    h_genMuTrkVy->Write();
    h_genMuTrkVz->Write();
    h_genMuTrkVabs->Write();
    h_genMuVtxChi2Ndof->Write();
    h_genMuVtxAngleXY->Write();
    h_genMuVtxAngleXYZ->Write();
    h_genMuVtxSigXY->Write();
    h_genMuVtxSigXYZ->Write();
    h_genMuTrkDca->Write();

    h_combMuVtxPx->Write();
    h_combMuVtxPy->Write();
    h_combMuVtxPz->Write();
    h_combMuVtxP2->Write();
    h_combMuTrkVx->Write();
    h_combMuTrkVy->Write();
    h_combMuTrkVz->Write();
    h_combMuTrkVabs->Write();
    h_combMuVtxChi2Ndof->Write();
    h_combMuVtxAngleXY->Write();
    h_combMuVtxAngleXYZ->Write();
    h_combMuVtxSigXY->Write();
    h_combMuVtxSigXYZ->Write();
    h_combMuTrkDca->Write();

    h_fakeMuVtxPx->Write();
    h_fakeMuVtxPy->Write();
    h_fakeMuVtxPz->Write();
    h_fakeMuVtxP2->Write();
    h_fakeMuTrkVx->Write();
    h_fakeMuTrkVy->Write();
    h_fakeMuTrkVz->Write();
    h_fakeMuTrkVabs->Write();
    h_fakeMuVtxChi2Ndof->Write();
    h_fakeMuVtxAngleXY->Write();
    h_fakeMuVtxAngleXYZ->Write();
    h_fakeMuVtxSigXY->Write();
    h_fakeMuVtxSigXYZ->Write();
    h_fakeMuTrkDca->Write();

    h_originalGenMuTrkPt1->Write();
    h_originalGenMuTrkEta1->Write();
    h_originalGenMuTrkChi2Ndof1->Write();
    h_refittedGenMuTrkPt1->Write();
    h_refittedGenMuTrkEta1->Write();
    h_refittedGenMuTrkChi2Ndof1->Write();

    h_originalFakeMuTrkPt1->Write();
    h_originalFakeMuTrkEta1->Write();
    h_originalFakeMuTrkChi2Ndof1->Write();
    h_refittedFakeMuTrkPt1->Write();
    h_refittedFakeMuTrkEta1->Write();
    h_refittedFakeMuTrkChi2Ndof1->Write();

    h_originalGenMuTrkPt2->Write();
    h_originalGenMuTrkEta2->Write();
    h_originalGenMuTrkChi2Ndof2->Write();
    h_refittedGenMuTrkPt2->Write();
    h_refittedGenMuTrkEta2->Write();
    h_refittedGenMuTrkChi2Ndof2->Write();

    h_originalFakeMuTrkPt2->Write();
    h_originalFakeMuTrkEta2->Write();
    h_originalFakeMuTrkChi2Ndof2->Write();
    h_refittedFakeMuTrkPt2->Write();
    h_refittedFakeMuTrkEta2->Write();
    h_refittedFakeMuTrkChi2Ndof2->Write();

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

std::vector<int> getPromptMuons(const AnalysisEvent& event, const std::vector<int>& muonIndex, const bool getPrompt ) {
    std::vector<int> muons;
    for ( auto it = muonIndex.begin(); it!= muonIndex.end(); it++ ) {
        if ( event.genMuonPF2PATHardProcess[*it] == getPrompt ) muons.push_back(*it);
    }
    return muons;
}

bool getDileptonCand(AnalysisEvent& event, const std::vector<int>& muons, const float skMass, const bool exactlyTwo) {
    if (exactlyTwo && muons.size() == 2) {

        if (event.muonPF2PATCharge[muons[0]] * event.muonPF2PATCharge[muons[1]] >= 0) return false;

        TLorentzVector lepton1{event.muonPF2PATPX[muons[0]],
                               event.muonPF2PATPY[muons[0]],
                               event.muonPF2PATPZ[muons[0]],
                               event.muonPF2PATE[muons[0]]};
        TLorentzVector lepton2{event.muonPF2PATPX[muons[1]],
                               event.muonPF2PATPY[muons[1]],
                               event.muonPF2PATPZ[muons[1]],
                               event.muonPF2PATE[muons[1]]};

        event.zPairLeptons.first = lepton1.Pt() > lepton2.Pt() ? lepton1 : lepton2;
        event.zPairIndex.first = lepton1.Pt() > lepton2.Pt() ? muons[0] : muons[1];
        event.zPairRelIso.first = event.muonPF2PATComRelIsodBeta[muons[0]];
        event.zPairLeptons.second = lepton1.Pt() > lepton2.Pt() ? lepton2 : lepton1;
        event.zPairRelIso.second = event.muonPF2PATComRelIsodBeta[muons[1]];
        event.zPairIndex.second = lepton1.Pt() > lepton2.Pt() ? muons[1] : muons[0];
        return true;
    }
    else if (!exactlyTwo && muons.size() > 1) {
        double closestMass {9999.};
        for ( unsigned int i{0}; i < muons.size(); i++ ) {
            for ( unsigned int j{i+1}; j < muons.size(); j++ ) {
                if (event.muonPF2PATCharge[i] * event.muonPF2PATCharge[j] >= 0) continue;
                TLorentzVector lepton1{event.muonPF2PATPX[i],
                                       event.muonPF2PATPY[i],
                                       event.muonPF2PATPZ[i],
                                       event.muonPF2PATE[i]};
                TLorentzVector lepton2{event.muonPF2PATPX[j],
                                       event.muonPF2PATPY[j],
                                       event.muonPF2PATPZ[j],
                                       event.muonPF2PATE[j]};
                double invMass { (lepton1+lepton2).M() };
                if ( std::abs(( invMass - skMass )) < std::abs(closestMass) ) {
                    event.zPairLeptons.first = lepton1.Pt() > lepton2.Pt() ? lepton1 : lepton2;
                    event.zPairIndex.first = lepton1.Pt() > lepton2.Pt() ? muons[0] : muons[1];
                    event.zPairRelIso.first = event.muonPF2PATComRelIsodBeta[muons[0]];
                    event.zPairLeptons.second = lepton1.Pt() > lepton2.Pt() ? lepton2 : lepton1;
                    event.zPairRelIso.second = event.muonPF2PATComRelIsodBeta[muons[1]];
                    event.zPairIndex.second = lepton1.Pt() > lepton2.Pt() ? muons[1] : muons[0];
                    closestMass = ( invMass - skMass );
                }
            }
        }
        if ( closestMass < 9999. ) return true;
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
    else if (motherIndex > event.NGENPARMAX) return false; // index exceeds stored genParticle range, return false for safety
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
