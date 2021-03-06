// ################################################################
// Author:  Joel Mazer for the STAR Collaboration
// Affiliation: Rutgers University
//
// track and tower input to cluster over and create jets
//      - leading jet tag
//      - access to jet constituents
//      - general QA
//
// ################################################################
// $Id$

#include "StJetMakerTask.h"

// root classes
#include <TChain.h>
#include <TClonesArray.h>
#include "TH1F.h"
#include "TH2F.h"
#include <TList.h>
#include <TLorentzVector.h>
#include <TParticle.h>
#include "TFile.h"

#include <sstream>
#include <fstream>

// general StRoot classes
#include "StThreeVectorF.hh"

// StRoot classes
#include "StRoot/StPicoDstMaker/StPicoDst.h"
#include "StRoot/StPicoDstMaker/StPicoDstMaker.h"
#include "StRoot/StPicoDstMaker/StPicoArrays.h"
#include "StRoot/StPicoEvent/StPicoEvent.h"
#include "StRoot/StPicoEvent/StPicoTrack.h"
#include "StPicoEvent/StPicoEmcTrigger.h"
#include "StPicoEvent/StPicoBTowHit.h"
#include "StPicoEvent/StPicoBEmcPidTraits.h"

#include "StPicoConstants.h"

// for clusters
#include "StMuDSTMaker/COMMON/StMuTrack.h"
//StEmc
#include "StEmcClusterCollection.h"
#include "StEmcCollection.h"
#include "StEmcCluster.h"
#include "StEmcPoint.h"
#include "StEmcUtil/geometry/StEmcGeom.h"
////#include "StEmcUtil/others/emcDetectorName.h" ?
#include "StEmcUtil/projection/StEmcPosition.h"
class StEmcPosition;
class StEEmcCluster;

// jet class and fastjet wrapper
#include "StJet.h"
#include "StFJWrapper.h"
#include "StJetFrameworkPicoBase.h"

// centrality
#include "StRoot/StRefMultCorr/StRefMultCorr.h"
#include "StRoot/StRefMultCorr/CentralityMaker.h"

#include "StJetPicoDefinitions.h"

class StMaker;
class StChain;
class StPicoDstMaker;
class StPicoEvent;

const Int_t StJetMakerTask::fgkConstIndexShift = 100000;

ClassImp(StJetMakerTask)

//________________________________________________________________________
StJetMakerTask::StJetMakerTask() : 
  StMaker(),
  doWriteHistos(kFALSE),
  doUsePrimTracks(kFALSE), 
  fDebugLevel(0),
  fRunFlag(0),       // see StJetFrameworkPicoBase::fRunFlagEnum
  fCentralityDef(4), // see StJetFrameworkPicoBase::fCentralityDefEnum
  fRequireCentSelection(kFALSE),
  fEventZVtxMinCut(-40.0), 
  fEventZVtxMaxCut(40.0),
  fCentralitySelectionCut(-99),
  doUseBBCCoincidenceRate(kTRUE),
  Bfield(0.0),
  mVertex(0x0),
  zVtx(0.0),
  fEmcTriggerEventType(0), // see StJetFrameworkPicoBase::fEmcTriggerFlagEnum
  fMBEventType(2),         // kVPDMB5, see StJetFrameworkPicoBase::fMBFlagEnum
  fTriggerToUse(0),        // kTriggerAny, see StJetFrameworkPicoBase::fTriggerEventTypeEnum
  fBadTowerListVers(1),
  mOutName(""),
  fTracksName(""),
  fCaloName(""),
  fJetsName(""),
  fJetAlgo(1), 
  fJetType(0), 
  fRecombScheme(fastjet::BIpt_scheme),
  fjw("StJetMakerTask", "StJetMakerTask"),
  fRadius(0.4),
  fMinJetArea(0.001),
  fMinJetPt(1.0),
  fJetPhiMin(-10.),
  fJetPhiMax(+10.),
  fJetEtaMin(-0.6),
  fJetEtaMax(0.6),
  fGhostArea(0.005), 
  fMinJetTrackPt(0.2),
  fMaxJetTrackPt(20.0),
  fMinJetClusPt(0.15),
  fMinJetClusE(0.2),
  fMinJetTowerE(0.2),
  fJetTrackEtaMin(-1.0),
  fJetTrackEtaMax(1.0),
  fJetTrackPhiMin(0.0),
  fJetTrackPhiMax(2.0*TMath::Pi()),
  fJetTrackDCAcut(3.0),
  fJetTracknHitsFit(15),
  fJetTracknHitsRatio(0.52),
  fTrackEfficiency(1.),
  fJetTowerEMin(0.2),
  fJetTowerEMax(100.0),
  fJetTowerEtaMin(-1.0),
  fJetTowerEtaMax(1.0),
  fJetTowerPhiMin(0.0),
  fJetTowerPhiMax(2.0*TMath::Pi()),
  mTowerEnergyMin(0.2),
  mHadronicCorrFrac(1.),
  fLegacyMode(kFALSE),
  fFillGhost(kFALSE),
  fJets(0x0),
  fConstituents(0),
  fJetsConstit(0x0),
  mGeom(StEmcGeom::instance("bemc")),
  mEmcCol(0),
  mu(0x0),
  mPicoDstMaker(0x0),
  mPicoDst(0x0),
  mPicoEvent(0x0),
  grefmultCorr(0x0)
//  fJetMakerName("")
{
  // Default constructor.
  for(int i=0; i<8; i++) { fEmcTriggerArr[i] = kFALSE; }

  for(int i=0; i<4801; i++) { 
    fTowerToTriggerTypeHT1[i] = kFALSE;
    fTowerToTriggerTypeHT2[i] = kFALSE;
    fTowerToTriggerTypeHT3[i] = kFALSE; 

    mTowerMatchTrkIndex[i] = 0;
    mTowerStatusArr[i] = kFALSE;
  }

}

//________________________________________________________________________
StJetMakerTask::StJetMakerTask(const char *name, double mintrackPt = 0.20, bool doHistos = kFALSE, const char* outName = "") : 
  StMaker(name),
  doWriteHistos(doHistos),
  doUsePrimTracks(kFALSE),
  fDebugLevel(0),
  fRunFlag(0),       // see StJetFrameworkPicoBase::fRunFlagEnum
  fCentralityDef(4), // see StJetFrameworkPicoBase::fCentralityDefEnum
  fRequireCentSelection(kFALSE),
  fEventZVtxMinCut(-40.0), 
  fEventZVtxMaxCut(40.0),
  fCentralitySelectionCut(-99),
  doUseBBCCoincidenceRate(kTRUE),
  Bfield(0.0),
  mVertex(0x0),
  zVtx(0.0),
  fEmcTriggerEventType(0), // see StJetFrameworkPicoBase::fEMCTriggerFlagEnum
  fMBEventType(2),         // kVPDMB5, see StJetFrameworkPicoBase::fMBFlagEnum
  fTriggerToUse(0),        // kTriggerAny, see StJetFrameworkPicoBase::fTriggerEventTypeEnum
  fBadTowerListVers(1),
  mOutName(outName),
  fTracksName("Tracks"),
  fCaloName("Clusters"),
  fJetsName("Jets"),
  fJetAlgo(1), 
  fJetType(0),
  fRecombScheme(fastjet::BIpt_scheme),
  fjw(name, name),
  fRadius(0.4),
  fMinJetArea(0.001),
  fMinJetPt(1.0),
  fJetPhiMin(-10), 
  fJetPhiMax(+10),
  fJetEtaMin(-0.6), 
  fJetEtaMax(0.6),
  fGhostArea(0.005),
  fMinJetTrackPt(mintrackPt), //0.20
  fMaxJetTrackPt(20.0), 
  fMinJetClusPt(0.15),
  fMinJetClusE(0.2),
  fMinJetTowerE(0.2),
  fJetTrackEtaMin(-1.0), 
  fJetTrackEtaMax(1.0),
  fJetTrackPhiMin(0.0),
  fJetTrackPhiMax(2.0*TMath::Pi()),
  fJetTrackDCAcut(3.0),
  fJetTracknHitsFit(15),
  fJetTracknHitsRatio(0.52),
  fTrackEfficiency(1.),
  fJetTowerEMin(0.2),
  fJetTowerEMax(100.0),
  fJetTowerEtaMin(-1.0),
  fJetTowerEtaMax(1.0),
  fJetTowerPhiMin(0.0),
  fJetTowerPhiMax(2.0*TMath::Pi()),
  mTowerEnergyMin(0.2),
  mHadronicCorrFrac(1.),
  fLegacyMode(kFALSE),
  fFillGhost(kFALSE),
  fJets(0x0),
  fConstituents(0),
  fJetsConstit(0x0),
  mGeom(StEmcGeom::instance("bemc")),
  mEmcCol(0),
  mu(0x0),
  mPicoDstMaker(0x0),
  mPicoDst(0x0),
  mPicoEvent(0x0),
  grefmultCorr(0x0)
//  fJetMakerName("")
{
  // Standard constructor.
  for(int i=0; i<8; i++) { fEmcTriggerArr[i] = kFALSE; }

  for(int i=0; i<4801; i++) {
    fTowerToTriggerTypeHT1[i] = kFALSE;
    fTowerToTriggerTypeHT2[i] = kFALSE;
    fTowerToTriggerTypeHT3[i] = kFALSE;

    mTowerMatchTrkIndex[i] = 0;
    mTowerStatusArr[i] = kFALSE;
  }

  if (!name) return;
  SetName(name);
}

//________________________________________________________________________
StJetMakerTask::~StJetMakerTask()
{
  // Destructor
  //fJets->Clear(); delete fJets;
  if(fHistCentrality)          delete fHistCentrality;

  if(fHistJetNTrackvsPt)       delete fHistJetNTrackvsPt;
  if(fHistJetNTrackvsPhi)      delete fHistJetNTrackvsPhi;
  if(fHistJetNTrackvsEta)      delete fHistJetNTrackvsEta;
  if(fHistJetNTrackvsPhivsEta) delete fHistJetNTrackvsPhivsEta;
  if(fHistJetNTowervsID)       delete fHistJetNTowervsID;
  if(fHistJetNTowervsE)        delete fHistJetNTowervsE;
  if(fHistJetNTowervsEt)       delete fHistJetNTowervsEt;
  if(fHistJetNTowervsPhi)      delete fHistJetNTowervsPhi;
  if(fHistJetNTowervsEta)      delete fHistJetNTowervsEta;
  if(fHistJetNTowervsPhivsEta) delete fHistJetNTowervsPhivsEta;

  if(fHistNJetsvsPt)           delete fHistNJetsvsPt;
  if(fHistNJetsvsPhi)          delete fHistNJetsvsPhi;
  if(fHistNJetsvsEta)          delete fHistNJetsvsEta;
  if(fHistNJetsvsPhivsEta)     delete fHistNJetsvsPhivsEta;
  if(fHistNJetsvsArea)         delete fHistNJetsvsArea;
  if(fHistNJetsvsNConstituents)delete fHistNJetsvsNConstituents;
  if(fHistNJetsvsNTracks)      delete fHistNJetsvsNTracks;
  if(fHistNJetsvsNTowers)      delete fHistNJetsvsNTowers;

  if(fHistQATowIDvsEta)        delete fHistQATowIDvsEta;
  if(fHistQATowIDvsPhi)        delete fHistQATowIDvsPhi;
}

//
//________________________________________________________________________
Int_t StJetMakerTask::Init() {
  DeclareHistograms();

  //AddBadTowers( TString( getenv("STARPICOPATH" )) + "/badTowerList_y11.txt");
  // Add dead + bad tower lists
  switch(fRunFlag) {
    case StJetFrameworkPicoBase::Run14_AuAu200 : // Run14 AuAu
        //AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_BadTowers.txt");
        //AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_AltBadTowers.txt");
        if(fBadTowerListVers ==  1)  AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_BadTowers.txt");   // original default
        if(fBadTowerListVers ==  2)  AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_AltBadTowers.txt");// Alt list
        if(fBadTowerListVers ==  3)  AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_AltBadTowers3.txt");// Alt list
        if(fBadTowerListVers ==  79) AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_AltBadTowers_79.txt");
        if(fBadTowerListVers == 122) AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_AltBadTowers_79_ALT.txt");
        if(fBadTowerListVers == 283) AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_AltBadTowers_283.txt");

        if(fBadTowerListVers == 50)  AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_BadTowers50.txt");// 50x from ped cut
        if(fBadTowerListVers == 51)  AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_BadTowers50_ALT.txt");// 50x + some manually added
        if(fBadTowerListVers == 5)  AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_BadTowers5.txt");

        AddDeadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2014_DeadTowers.txt");
        break;

    case StJetFrameworkPicoBase::Run16_AuAu200 : // Run16 AuAu
        AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2016_BadTowers.txt");
        AddDeadTowers("StRoot/StMyAnalysisMaker/towerLists/Y2016_DeadTowers.txt");
        break;

    default :
      AddBadTowers("StRoot/StMyAnalysisMaker/towerLists/Empty_BadTowers.txt");
      AddDeadTowers("StRoot/StMyAnalysisMaker/towerLists/Empty_DeadTowers.txt");
  }

  // Create user objects.
  fJets = new TClonesArray("StJet");
  fJets->SetName(fJetsName);

  // may need array (name hard-coded, Feb20, 2018)
  fJetsConstit = new TClonesArray("StPicoTrack");
  fJetsConstit->SetName("JetConstituents");

  // ============================ set jet parameters for fastjet wrapper  =======================
  // recombination schemes:
  // E_scheme, pt_scheme, pt2_scheme, Et_scheme, Et2_scheme, BIpt_scheme, BIpt2_scheme, WTA_pt_scheme, WTA_modp_scheme
  fastjet::RecombinationScheme    recombScheme;
  if (fRecombScheme == 0)     recombScheme = fastjet::E_scheme;
  if (fRecombScheme == 1)     recombScheme = fastjet::pt_scheme; 
  if (fRecombScheme == 2)     recombScheme = fastjet::pt2_scheme;
  if (fRecombScheme == 3)     recombScheme = fastjet::Et_scheme;
  if (fRecombScheme == 4)     recombScheme = fastjet::Et2_scheme;
  if (fRecombScheme == 5)     recombScheme = fastjet::BIpt_scheme;
  if (fRecombScheme == 6)     recombScheme = fastjet::BIpt2_scheme;
  if (fRecombScheme == 7)     recombScheme = fastjet::WTA_pt_scheme;
  if (fRecombScheme == 8)     recombScheme = fastjet::WTA_modp_scheme;
  if (fRecombScheme == 99)    recombScheme = fastjet::external_scheme;

  // jet algorithm
  fastjet::JetAlgorithm          algorithm;
  if (fJetAlgo == 1)      algorithm = fastjet::antikt_algorithm;
  if (fJetAlgo == 0)      algorithm = fastjet::kt_algorithm;
  // extra algorithms
  if (fJetAlgo == 2)      algorithm = fastjet::cambridge_algorithm;
  if (fJetAlgo == 3)      algorithm = fastjet::genkt_algorithm;
  if (fJetAlgo == 11)     algorithm = fastjet::cambridge_for_passive_algorithm;
  if (fJetAlgo == 13)     algorithm = fastjet::genkt_for_passive_algorithm;
  if (fJetAlgo == 99)     algorithm = fastjet::plugin_algorithm;
  if (fJetAlgo == 999)    algorithm = fastjet::undefined_jet_algorithm;
  fastjet::Strategy               strategy = fastjet::Best;

  // setup fj wrapper
  fjw.SetAreaType(fastjet::active_area_explicit_ghosts);
  fjw.SetStrategy(strategy);
  fjw.SetGhostArea(fGhostArea);
  fjw.SetR(fRadius);
  fjw.SetAlgorithm(algorithm);        //fJetAlgo);
  fjw.SetRecombScheme(recombScheme);  //fRecombScheme);
  fjw.SetMaxRap(1);

  // setting legacy mode
  //if(fLegacyMode) { fjw.SetLegacyMode(kTRUE); }

  // may not need, used for old RUNS
  // StRefMultCorr* getgRefMultCorr() ; // For grefmult //Run14 AuAu200GeV
  // switch on Run Flag to look for firing trigger specifically requested for given run period
  switch(fRunFlag) {
    case StJetFrameworkPicoBase::Run14_AuAu200 : // Run14 AuAu
        // this is the default for Run14
        grefmultCorr = CentralityMaker::instance()->getgRefMultCorr();        
        break;

    case StJetFrameworkPicoBase::Run16_AuAu200 : // Run16 AuAu
        switch(fCentralityDef) {      
          case StJetFrameworkPicoBase::kgrefmult :
              grefmultCorr = CentralityMaker::instance()->getgRefMultCorr();
              break;
          case StJetFrameworkPicoBase::kgrefmult_P16id :
              grefmultCorr = CentralityMaker::instance()->getgRefMultCorr_P16id();
              break;
          case StJetFrameworkPicoBase::kgrefmult_VpdMBnoVtx : 
              grefmultCorr = CentralityMaker::instance()->getgRefMultCorr_VpdMBnoVtx();
              break;
          case StJetFrameworkPicoBase::kgrefmult_VpdMB30 : 
              grefmultCorr = CentralityMaker::instance()->getgRefMultCorr_VpdMB30();
              break;
          default:
              grefmultCorr = CentralityMaker::instance()->getgRefMultCorr_P16id();
        }
        break;  // need this from embedded switch - May20
    
    default :
        cout<<"I am in default.. - THIS SHOULD NOT HAPPEN!!"<<endl;
        grefmultCorr = CentralityMaker::instance()->getgRefMultCorr();
        break;
  }

  return kStOK;
}

//
//_______________________________________________________________________________________
Int_t StJetMakerTask::Finish() {
  //  Summarize the run.
/*
  cout<<"StJetMakerTask::Finish()\n";
  cout<<"##### Jet parameters overview #####"<<endl;
  cout<<"type = "<<"   algorithm = "<<"  recombination scheme = "<<endl;
  cout<<"R = "<<fRadius<<"   ghostArea = "<<fGhostArea<<"  minJetArea = "<<fMinJetArea<<endl;
  cout<<"minTrackPt = "<<fMinJetTrackPt<<"  minClusterPt = "<<fMinJetClusPt<<"  maxTrackPt = "<<fMaxJetTrackPt<<endl;
  cout<<"minJetPhi = "<<fJetPhiMin<<"  maxJetPhi = "<<fJetPhiMax<<"  minJetEta = "<<fJetEtaMin<<"  maxJetEta = "<<fJetEtaMax<<endl;
  cout<<"End of StJetMakerTask::Finish"<<endl;
*/

  if(doWriteHistos && mOutName!="") {
    TFile *fout = new TFile(mOutName.Data(), "UPDATE");
    fout->cd();
    fout->mkdir(GetName());
    fout->cd(GetName());
    WriteHistograms();
    fout->cd();
    fout->Write();
    fout->Close();
  }

/*   ===== test ======
  if(mOutName!="") {
    cout<<"checking output file in StJetMakerTask::Finish().."<<endl;
    TFile *fout = new TFile(mOutName.Data(),"UPDATE");
    fout->cd();
    fout->mkdir(GetName());
    fout->cd(GetName());
    WriteHistograms();
    fout->cd();
    fout->Close();
  }
*/

  return kStOK;
}

//________________________________________________________________________
void StJetMakerTask::DeclareHistograms() {
    // declare histograms
    double pi = 1.0*TMath::Pi();

    //fHistEventCounter = new TH1F("fHistEventCounter", "Event counter", 10, 0.5, 10.5);
    fHistCentrality = new TH1F("fHistCentrality", "No. events vs centrality", 20, 0, 100);    

    fHistJetNTrackvsPt = new TH1F("fHistJetNTrackvsPt", "Jet track constituents vs p_{T}", 100, 0., 20.);
    fHistJetNTrackvsPhi = new TH1F("fHistJetNTrackvsPhi", "Jet track constituents vs #phi", 72, 0., 2*pi);
    fHistJetNTrackvsEta = new TH1F("fHistJetNTrackvsEta", "Jet track constituents vs #eta", 40, -1.0, 1.0);
    fHistJetNTrackvsPhivsEta = new TH2F("fHistJetNTrackvsPhivsEta", "Jet track constituents vs #phi vs #eta", 144, 0, 2*pi, 20, -1.0, 1.0);
    fHistJetNTowervsID = new TH1F("fHistJetNTowervsID", "Jet tower vs tower id", 4800, 0.5, 4800.5);
    fHistJetNTowervsE = new TH1F("fHistJetNTowervsE", "Jet tower constituents vs energy", 100, 0., 20.0);
    fHistJetNTowervsEt = new TH1F("fHistJetNTowervsEt", "Jet tower constituents vs transverse energy", 100, 0., 20.0);
    fHistJetNTowervsPhi = new TH1F("fHistJetNTowervsPhi", "Jet tower constituents vs #phi", 144, 0., 2*pi);
    fHistJetNTowervsEta = new TH1F("fHistJetNTowervsEta", "Jet tower constituents vs #eta", 40, -1.0, 1.0);
    fHistJetNTowervsPhivsEta = new TH2F("fHistJetNTowervsPhivsEta", "Jet tower constituents vs #phi vs #eta", 144, 0, 2*pi, 20, -1.0, 1.0);

    fHistNJetsvsPt = new TH1F("fHistNJetsvsPt", "NJets vs p_{T}", 100, 0., 100.0);
    fHistNJetsvsPhi = new TH1F("fHistNJetsvsPhi", "NJets vs #phi", 144, 0., 2*pi);
    fHistNJetsvsEta = new TH1F("fHistNJetsvsEta", "NJets vs #eta", 40, -1.0, 1.0);
    fHistNJetsvsPhivsEta = new TH2F("fHistNJetsvsPhivsEta", "NJets vs #phi vs #eta", 144, 0, 2*pi, 20, -1.0, 1.0);
    fHistNJetsvsArea = new TH1F("fHistNJetsvsArea", "NJets vs jet area", 100, 0.0, 1.0);
    fHistNJetsvsNConstituents = new TH1F("fHistNJetsvsNConstituents", "NJets vs NConstit", 51, -0.5, 50.5);
    fHistNJetsvsNTracks = new TH1F("fHistNJetsvsNTracks", "NJets vs NTracks", 50, -0.5, 50.5);
    fHistNJetsvsNTowers = new TH1F("fHistNJetsvsNTowers", "NJets vs NTowers", 50, -0.5, 50.5);

    fHistQATowIDvsEta = new TH2F("fHistQATowIDvsEta", "Tower ID vs #eta", 4800, 0.5, 4800.5, 40, -1.0, 1.0);
    fHistQATowIDvsPhi = new TH2F("fHistQATowIDvsPhi", "Tower ID vs #phi", 4800, 0.5, 4800.5, 144, 0.0, 2.*pi);
}

//________________________________________________________________________

void StJetMakerTask::WriteHistograms() {
  // write histograms
  fHistCentrality->Write();

  fHistJetNTrackvsPt->Write();
  fHistJetNTrackvsPhi->Write();
  fHistJetNTrackvsEta->Write();
  fHistJetNTrackvsPhivsEta->Write();
  fHistJetNTowervsID->Write();
  fHistJetNTowervsE->Write();
  fHistJetNTowervsEt->Write();
  fHistJetNTowervsPhi->Write();
  fHistJetNTowervsEta->Write();
  fHistJetNTowervsPhivsEta->Write();

  fHistNJetsvsPt->Write();
  fHistNJetsvsPhi->Write();
  fHistNJetsvsEta->Write();
  fHistNJetsvsPhivsEta->Write();
  fHistNJetsvsArea->Write();
  fHistNJetsvsNConstituents->Write();
  fHistNJetsvsNTracks->Write();
  fHistNJetsvsNTowers->Write();

  fHistQATowIDvsEta->Write();
  fHistQATowIDvsPhi->Write();
}

//-----------------------------------------------------------------------------
void StJetMakerTask::Clear(Option_t *opt) {
  // clear or delete objects after running
  fJets->Clear();
}

//________________________________________________________________________
int StJetMakerTask::Make()
{
  // Main loop, called for each event.
  // ZERO's out the jet array
  fJets->Delete();

  // April9 test - ZERO these out for double checking they aren't set
  for(int i = 0; i < 4801; i++) {
    mTowerMatchTrkIndex[i] = 0;
    mTowerStatusArr[i] = kFALSE;
  }

  // get PicoDstMaker 
  mPicoDstMaker = static_cast<StPicoDstMaker*>(GetMaker("picoDst"));
  if(!mPicoDstMaker) {
    LOG_WARN << " No PicoDstMaker! Skip! " << endm;
    return kStWarn;
  }

  // construct PicoDst object from maker
  mPicoDst = static_cast<StPicoDst*>(mPicoDstMaker->picoDst());
  if(!mPicoDst) {
    LOG_WARN << " No PicoDst! Skip! " << endm;
    return kStWarn;
  }

  // create pointer to PicoEvent 
  mPicoEvent = static_cast<StPicoEvent*>(mPicoDst->event());
  if(!mPicoEvent) {
    LOG_WARN << " No PicoEvent! Skip! " << endm;
    return kStWarn;
  }

  // get event B (magnetic) field
  Bfield = mPicoEvent->bField();

  // get vertex 3-vector and z-vertex component
  mVertex = mPicoEvent->primaryVertex();
  zVtx = mVertex.z();

  // Z-vertex cut - - per the Aj analysis (-40, 40)
  // TEST: kStOk -> kStErr // Error, drop this and go to the next event
  if((zVtx < fEventZVtxMinCut) || (zVtx > fEventZVtxMaxCut)) return kStOk; //Pico::kSkipThisEvent;

  // ============================ CENTRALITY ============================== //
  // 10 14 21 29 40 54 71 92 116 145 179 218 263 315 373 441  // RUN 14 AuAu binning
  int RunId = mPicoEvent->runId();
  double fBBCCoincidenceRate = mPicoEvent->BBCx();
  double fZDCCoincidenceRate = mPicoEvent->ZDCx();
  int grefMult = mPicoEvent->grefMult();
  grefmultCorr->init(RunId);
  if(doUseBBCCoincidenceRate) { grefmultCorr->initEvent(grefMult, zVtx, fBBCCoincidenceRate); } // default
  else{ grefmultCorr->initEvent(grefMult, zVtx, fZDCCoincidenceRate); }
  grefmultCorr->initEvent(grefMult, zVtx, fBBCCoincidenceRate);
  grefmultCorr->getRefMultCorr(grefMult, zVtx, fBBCCoincidenceRate, 2);
  int cent16 = grefmultCorr->getCentralityBin16();
  if(cent16 == -1) return kStOk; // - this is for lowest multiplicity events 80%+ centrality, cut on them
  int centbin = GetCentBin(cent16, 16);
  double centralityScaled = centbin*5.0;
  fHistCentrality->Fill(centralityScaled);

  // cut on centrality for analysis before doing anything
  // TEST: kStOk -> kStErr // Error, drop this and go to the next event
  if(fRequireCentSelection) { if(!SelectAnalysisCentralityBin(centbin, fCentralitySelectionCut)) return kStOk; } // Pico::kSkipThisEvent; }

  // check for MB and HT triggers - Type Flag corresponds to selected type of MB or EMC
  bool fHaveMBevent = CheckForMB(fRunFlag, fMBEventType);
  bool fHaveEmcTrigger = CheckForHT(fRunFlag, fEmcTriggerEventType);
  //bool fHaveAnyEvent = kTRUE;

  // fill trigger array
  FillEmcTriggersArr();

  // no need for switch for few checks
  if((fTriggerToUse == StJetFrameworkPicoBase::kTriggerMB) && (!fHaveMBevent))   return kStOK;  // MB triggered event
  if((fTriggerToUse == StJetFrameworkPicoBase::kTriggerHT) && (!fHaveEmcTrigger))return kStOK;  // HT triggered event
  // else fTriggerToUse is ANY and we still want to run analysis

  // Find jets:  deprecated version -> FindJets(tracks, clus, fJetAlgo, fRadius);
  FindJets();

  // Fill jet branch
  FillJetBranch();

  return kStOK;
}

//
// old class to FindJets - it is deprecated, but kept for backwards compatibility
// the parameters are global so they don't do anything here
//________________________________________________________________________
void StJetMakerTask::FindJets(TObjArray *tracks, TObjArray *clus, Int_t algo, Double_t radius)
{
  // call main (new) FindJets function
  FindJets();
}

//
// Main class to FindJets from tracks + towers
//________________________________________________________________________
void StJetMakerTask::FindJets()
{
  // Find jets.
  // clear out existing wrapper object
  fjw.Clear();

  // initialize Emc position objects
  StEmcPosition *mPosition = new StEmcPosition();

  // assume neutral pion mass
  // additional parameters constructed
  double pi = 1.0*TMath::Pi();
  double pi0mass = Pico::mMass[0]; // GeV
  unsigned int ntracks = mPicoDst->numberOfTracks();

  // loop over ALL tracks in PicoDst and add to jet, after acceptance and quality cuts 
  if((fJetType == kFullJet) || (fJetType == kChargedJet)) {
    for(unsigned short iTracks = 0; iTracks < ntracks; iTracks++){
      StPicoTrack* trk = static_cast<StPicoTrack*>(mPicoDst->track(iTracks));
      if(!trk){ continue; }

      // acceptance and kinematic quality cuts
      if(!AcceptJetTrack(trk, Bfield, mVertex)) { continue; }

      // get momentum vector of track - global or primary track
      StThreeVectorF mTrkMom;
      if(doUsePrimTracks) { 
        // get primary track vector
        mTrkMom = trk->pMom(); 
      } else { 
        // get global track vector
        mTrkMom = trk->gMom(mVertex, Bfield); 
      }

      // track variables
      //double pt = mTrkMom.perp();
      //double phi = mTrkMom.phi();
      //double eta = mTrkMom.pseudoRapidity();
      double px = mTrkMom.x();
      double py = mTrkMom.y();
      double pz = mTrkMom.z();
      double p = mTrkMom.mag();
      double energy = 1.0*TMath::Sqrt(p*p + pi0mass*pi0mass);
      //short charge = trk->charge();         

      // send track info to FJ wrapper
      //fjw.AddInputVector(px, py, pz, p, iTracks);    // p -> E
      fjw.AddInputVector(px, py, pz, energy, iTracks); // includes E
    } // track loop
  } // if full/charged jets

  // full or neutral jets - get towers and apply hadronic correction
  if((fJetType == kFullJet) || (fJetType == kNeutralJet)) {
    // ==================== March 6th, 2018
    // towerStatus array
    //float mTowerMatchTrkIndex[4801] = { 0 };
    //bool mTowerStatusArr[4801] = { 0 };
    int matchedTowerTrackCounter = 0;

    // print
    //int nTracks = mPicoDst->numberOfTracks();
    //int nTrigs = mPicoDst->numberOfEmcTriggers();
    //int nBTowHits = mPicoDst->numberOfBTOWHits();
    int nBEmcPidTraits = mPicoDst->numberOfBEmcPidTraits();
    //cout<<"nTracks = "<<nTracks<<"  nTrigs = "<<nTrigs<<"  nBTowHits = "<<nBTowHits<<"  nBEmcPidTraits = "<<nBEmcPidTraits<<endl;
  
    // loop over ALL clusters in PicoDst to get track<->tower matches saved to arrays for hadronic correction
    for(unsigned short iClus = 0; iClus < nBEmcPidTraits; iClus++){
      StPicoBEmcPidTraits* cluster = static_cast<StPicoBEmcPidTraits*>(mPicoDst->bemcPidTraits(iClus));
      if(!cluster){ cout<<"Cluster pointer does not exist.. iClus = "<<iClus<<endl; continue; }

      // cluster and tower ID
      // ID's are calculated as such:
      // mBtowId       = (ntow[0] <= 0 || ntow[0] > 4800) ? -1 : (Short_t)ntow[0];
      // mBtowId23 = (ntow[1] < 0 || ntow[1] >= 9 || ntow[2] < 0 || ntow[2] >= 9) ? -1 : (Char_t)(ntow[1] * 10 + ntow[2]);
      // commented out clusID, towID2, towID3 on May20
      //int clusID = cluster->bemcId();  // index in bemc point array
      int towID = cluster->btowId();   // projected tower Id: 1 - 4800
      //int towID2 = cluster->btowId2(); // emc 2nd and 3rd closest tower local id  ( 2nd X 10 + 3rd), each id 0-8
      //int towID3 = cluster->btowId3(); // emc 2nd and 3rd closest tower local id  ( 2nd X 10 + 3rd), each id 0-8
      if(towID < 0) continue;

      // cluster and tower position - from vertex and ID - shouldn't need to correct via this method
      ///StThreeVectorF towPosition = mPosition->getPosFromVertex(mVertex, towID);
      ///double towPhi = towPosition.phi();
      ///double towEta = towPosition.pseudoRapidity();

      // matched track index
      int trackIndex = cluster->trackIndex();
      StPicoTrack* trk = static_cast<StPicoTrack*>(mPicoDst->track(trackIndex));
      if(!trk) { cout<<"No trk pointer...."<<endl; continue; }

      // TODO should check this FIXME
      //if(!AcceptJetTrack(trk, Bfield, mVertex)) { continue; } // FIXME - do I want to apply quality cuts to matched track?
      if(AcceptJetTrack(trk, Bfield, mVertex)) {

        // tower status set - towerID is matched to track passing quality cuts
        mTowerMatchTrkIndex[towID] = trackIndex;
        mTowerStatusArr[towID] = kTRUE;
        matchedTowerTrackCounter++;
      } // when tracks meet cuts, save matching to arrays
    } // PIDTraits loop

    // loop over towers and add input vectors to fastjet
    int nTowers = mPicoDst->numberOfBTOWHits();
    for(int itow = 0; itow < nTowers; itow++) {
      StPicoBTowHit *tower = static_cast<StPicoBTowHit*>(mPicoDst->btowHit(itow));
      if(!tower) { cout<<"No tower pointer... iTow = "<<itow<<endl; continue; }

      // tower ID
      int towerID = tower->id();
      if(towerID < 0) continue; // double check these aren't still in the event list

      // cluster and tower position - from vertex and ID: shouldn't need additional eta correction
      StThreeVectorF towerPosition = mPosition->getPosFromVertex(mVertex, towerID);
      double towerPhi = towerPosition.phi();
      if(towerPhi < 0)    towerPhi += 2.0*pi;
      if(towerPhi > 2*pi) towerPhi -= 2.0*pi;
      double towerEta = towerPosition.pseudoRapidity();
      //int towerADC = tower->adc();
      double towerEunCorr = tower->energy();  // uncorrected energy
      double towerE = tower->energy();        // corrected energy (hadronically - done below)

      // tower matched to firing trigger - TODO
      //if(fTowerToTriggerTypeHT1[emcTrigID])
      //if(fTowerToTriggerTypeHT2[emcTrigID])
      //if(fTowerToTriggerTypeHT3[emcTrigID])

      // perform tower cuts
      // if tower was not matched to an accepted track, use it for jet by itself if > 0.2 GeV
      if(mTowerStatusArr[towerID]) {
        //if(mTowerMatchTrkIndex[towerID] > 0) 
        StPicoTrack* trk = static_cast<StPicoTrack*>(mPicoDst->track( mTowerMatchTrkIndex[towerID] ));
        if(!trk) { cout<<"No trk pointer...."<<endl; continue; }

        // TODO want to process tower on its own if track did not meet quality cut
        // this should already be done above
        //if(!AcceptJetTrack(trk, Bfield, mVertex)) { continue; }
        if(AcceptJetTrack(trk, Bfield, mVertex)) {

          // get track variables to matched tower
          StThreeVectorF mTrkMom;
          if(doUsePrimTracks) { 
            // get primary track vector
            mTrkMom = trk->pMom(); 
          } else { 
            // get global track vector
            mTrkMom = trk->gMom(mVertex, Bfield); 
          }

          // track variables
          //double pt = mTrkMom.perp();
          //double phi = mTrkMom.phi();
          //double eta = mTrkMom.pseudoRapidity();
          double p = mTrkMom.mag();
          double E = 1.0*TMath::Sqrt(p*p + pi0mass*pi0mass);

          // apply hadronic correction to tower
          towerE = towerEunCorr - (mHadronicCorrFrac * E);
        }  
      } 
      // else - no match so treat towers on their own

      // cut on transverse tower energy
      double towerEt = towerE / (1.0*TMath::CosH(towerEta));
      if(towerEt < 0) towerEt = 0.0;
      if(towerEt < mTowerEnergyMin) continue;

      // check for bad (and dead) towers
      bool TowerOK = IsTowerOK(towerID);
      bool TowerDead = IsTowerDead(towerID);
      if(!TowerOK) {  //cout<<"towerID bad = "<<towerID<<endl;
        continue;
      }
      //if(TowerDead) continue;

      //if(towerID == 796 || towerID == 1427 || towerID == 1984 || towerID == 2214 || towerID == 3488 || towerID == 3692 || towerID == 1125 || towerID == 1221) cout<<"towerID = "<<towerID<<"  E = "<<towerE<<"  eta = "<<towerEta<<"  phi = "<<towerPhi<<"  zVtx = "<<zVtx<<endl;

      // get components from Energy (p - momentum)
      // the below lines 'converts' the tower energy to momentum
      StThreeVectorF mom;
      GetMomentum(mom, tower, pi0mass);
      double towerPx = mom.x();
      double towerPy = mom.y();
      double towerPz = mom.z();

      // add towers to fastjet
      // shift tower index (tracks 0+, ghosts = -1, towers < -1)
      int uidTow = -(itow + 2);  
      fjw.AddInputVector(towerPx, towerPy, towerPz, towerE, uidTow); // includes E
    } // tower loop

// ====================
  } // neutral/full jets

  // run jet finder
  fjw.Run();

}

/**
 * This method fills the jet output branch (TClonesArray) with the jet found by the FastJet
 * wrapper. Before filling the jet branch, the utilities are prepared. Then the utilities are
 * called for each jet and finally after jet finding the terminate method of all utilities is called.
 */
void StJetMakerTask::FillJetBranch()
{
  std::vector<fastjet::PseudoJet> jets_incl = fjw.GetInclusiveJets();
  // sort jets according to jet pt
  static Int_t indexes[9999] = {-1};
  GetSortedArray(indexes, jets_incl);

  // loop over FastJet jets
  __DEBUG(StJetFrameworkPicoBase::kDebugFillJets, Form("%d jets found", (Int_t)jets_incl.size()));
  //for(UInt_t ij = 0, jetCount = 0; ij < jets_incl.size(); ++ij) {
  for(UInt_t ijet = 0, jetCount = 0; ijet < jets_incl.size(); ++ijet) {
    Int_t ij = indexes[ijet];
    __DEBUG(StJetFrameworkPicoBase::kDebugFillJets,Form("Jet pt = %f, area = %f", jets_incl[ij].perp(), fjw.GetJetArea(ij)));

    // PERFORM CUTS ON inclusive JETS before saving
    // cut on min jet pt
    if(jets_incl[ij].perp() < fMinJetPt) continue;
    // cut on min jet area
    if(fjw.GetJetArea(ij) < fMinJetArea*TMath::Pi()*fRadius*fRadius) continue;
    // cut on eta acceptance
    if((jets_incl[ij].eta() < fJetEtaMin) || (jets_incl[ij].eta() > fJetEtaMax)) continue;
    // cut on phi acceptance 
    if((jets_incl[ij].phi() < fJetPhiMin) || (jets_incl[ij].phi() > fJetPhiMax)) continue;

    // need to figure out how to get m or E from STAR tracks
    //StJet *jet = new ((*fJets)[jetCount]) 
    //  StJet(jets_incl[ij].perp(), jets_incl[ij].eta(), jets_incl[ij].phi(), jets_incl[ij].m());
    StJet *jet = new ((*fJets)[jetCount])
      StJet(jets_incl[ij].perp(), jets_incl[ij].eta(), jets_incl[ij].phi(), jets_incl[ij].m());

    jet->SetLabel(ij);

    // area vector and components
    fastjet::PseudoJet area(fjw.GetJetAreaVector(ij));
    jet->SetArea(area.perp());  // same as fjw.GetJetArea(ij)
    jet->SetAreaEta(area.eta());
    jet->SetAreaPhi(area.phi());
    jet->SetAreaE(area.E());

    // March 8, 2018 - probably don't need this anymore after coming up with method to pass and extract constituents via index!
    // get constituents of jets
    fConstituents = fjw.GetJetConstituents(ij);
    jet->SetJetConstituents(fConstituents);

    // fill jet constituents
    vector<fastjet::PseudoJet> constituents = fjw.GetJetConstituents(ij);
    FillJetConstituents(jet, constituents, constituents);

    __DEBUG(StJetFrameworkPicoBase::kDebugFillJets, Form("Added jet n. %d, pt = %f, area = %f, constituents = %d", jetCount, jet->Pt(), jet->Area(), jet->GetNumberOfConstituents()));

    jetCount++;
  } // jet loop 

}

/**
 * This method is called for each jet. It loops over the jet constituents and
 * adds them to the jet object.
 * @param jet Pointer to the AliEmcalJet object where the jet constituents will be added
 * @param constituents List of the jet constituents returned by the FastJet wrapper
 * @param constituents_unsub List of jet constituents before background subtraction
 * @param flag If kTRUE it means that the argument "constituents" is a list of subtracted constituents
 * @param particles_sub Array containing subtracted constituents - not used
 */
void StJetMakerTask::FillJetConstituents(StJet *jet, std::vector<fastjet::PseudoJet>& constituents,
    std::vector<fastjet::PseudoJet>& constituents_unsub, Int_t flag, TString particlesSubName)
{
  // initialize some variables/counters
  Double_t neutralE = 0, maxTrack = 0, maxTower = 0;
  Int_t nt = 0; // track counter
  Int_t nc = 0; // cluster counter
  Int_t ng = 0; // ghost counter  
  double pi = 1.0*TMath::Pi();
  double pi0mass = Pico::mMass[0]; // GeV

  // get EMCal position
  StEmcPosition *mPosition = new StEmcPosition();

  // initially set track and cluster constituent sizes
  jet->SetNumberOfTracks(constituents.size());
  jet->SetNumberOfClusters(constituents.size());

  // loop over constituents for ij'th jet
  for(UInt_t ic = 0; ic < constituents.size(); ++ic) {
    // get user defined index
    Int_t uid = constituents[ic].user_index();

    // CHARGED COMPONENT (tracks)
    if(uid >= 0) {
      jet->AddTrackAt(uid, nt);
      StPicoTrack* trk = static_cast<StPicoTrack*>(mPicoDst->track(uid));
      if(!trk) continue;

      // acceptance and kinematic quality cuts - probably not needed (done before passing to fastjet) FIXME
      if(!AcceptJetTrack(trk, Bfield, mVertex)) { continue; }

      // primary track switch
      // get momentum vector of track - global or primary track
      StThreeVectorF mTrkMom;
      if(doUsePrimTracks) {
        // get primary track vector
        mTrkMom = trk->pMom();
      } else {
        // get global track vector
        mTrkMom = trk->gMom(mVertex, Bfield);
      }

      // track variables
      double pt = mTrkMom.perp();
      double phi = mTrkMom.phi();
      double eta = mTrkMom.pseudoRapidity();

      // adjust phi value:  0 < phi < 2pi
      if(phi < 0)    phi += 2*pi;
      if(phi > 2*pi) phi -= 2*pi;

      // find max track pt
      if(pt > maxTrack) maxTrack = pt;

      // fill some QA histograms
      fHistJetNTrackvsPt->Fill(pt);
      fHistJetNTrackvsPhi->Fill(phi);
      fHistJetNTrackvsEta->Fill(eta);
      fHistJetNTrackvsPhivsEta->Fill(phi, eta);

      // increase track counter
      nt++;
    } else { // uid < 0

      // TODO - finish this if we ever care to look at ghosts.. (not a priority)
      // ghosts
      if(uid == -1) {
        //if(fFillGhost) jet->AddGhost(constituents[ic].px(), constituents[ic].py(), constituents[ic].pz(), constituents[ic].e());
        ng++;
      }

      // neutral towers: start at (index = -2, ghosts are -1, and tracks 0+)
      if(uid < -1) {
        // convert uid to tower index (index of tower)
        Int_t towIndex = -(uid + 2);   // 1 less than towerID
        jet->AddClusterAt(towIndex, nc);
        StPicoBTowHit *tower = static_cast<StPicoBTowHit*>(mPicoDst->btowHit(towIndex));
        if(!tower) continue;

        // cluster and tower position - from vertex and ID: shouldn't need additional eta correction
        int towerID = tower->id();
        StThreeVectorF towerPosition = mPosition->getPosFromVertex(mVertex, towerID);
        double towerPhi = towerPosition.phi();
        double towerEta = towerPosition.pseudoRapidity();
        double towEuncorr = tower->energy();
        double towE = tower->energy();

        // shift tower phi (0, 2*pi)
        if(towerPhi < 0)    towerPhi += 2*pi;
        if(towerPhi > 2*pi) towerPhi -= 2*pi;

        // April9, need to perform hadronic correction again since StBTowHit object is not updated
        // if tower was not matched to an accepted track, use it for jet by itself if > 0.2 GeV
        if(mTowerStatusArr[towerID]) {
          //if(mTowerMatchTrkIndex[towerID] > 0) 
          StPicoTrack* trk = static_cast<StPicoTrack*>(mPicoDst->track( mTowerMatchTrkIndex[towerID] ));
          if(!trk) { cout<<"No trk pointer...."<<endl; continue; }

          // TODO - want to check if pass cuts, but if not then don't correct tower, but use it
          //if(!AcceptJetTrack(trk, Bfield, mVertex)) { continue; }  // comment in April10
          if(AcceptJetTrack(trk, Bfield, mVertex)) {

            // get track variables to matched tower
            StThreeVectorF mTrkMom;
            if(doUsePrimTracks) {
              // get primary track vector
              mTrkMom = trk->pMom();
            } else {
              // get global track vector
              mTrkMom = trk->gMom(mVertex, Bfield);
            }

            // track variables
            double p = mTrkMom.mag();
            double E = 1.0*TMath::Sqrt(p*p + pi0mass*pi0mass);

            // apply hadronic correction to tower
            towE = towEuncorr - (mHadronicCorrFrac * E);
          }

        }
        // else - no match so treat towers on their own

        // cut on tower transverse energy - should of already been done before adding them to fastjet
        double towEt = towE / (1.0*TMath::CosH(towerEta)); // - FIXME should we cut on tower Et or E?
        if(towEt < 0) towEt = 0.0; 
        if(towEt < mTowerEnergyMin) continue;  // should make this Et TODO
        // =================================================================

        // find max tower E and neutral E total
        if(towE > maxTower) maxTower = towE;
        neutralE += towE;

        //if(fTowerToTriggerTypeHT1[towerID]) cout<<"firedTrigger TowerId = "<<towerID<<endl;
        //if(towerID == 796 || towerID == 1427 || towerID == 1984 || towerID == 2214 || toweirID == 3488 || towerID == 3692 || towerID == 1125 || towerID == 1221) cout<<"Adding to jet now.. towerID = "<<towerID<<"  E = "<<towE<<"  eta = "<<towerEta<<"  phi = "<<towerPhi<<"  zVtx = "<<zVtx<<endl;

        // fill QA histos for jet towers
        fHistJetNTowervsID->Fill(towerID);
        fHistJetNTowervsE->Fill(towE);
        fHistJetNTowervsEt->Fill(towEt);
        fHistJetNTowervsPhi->Fill(towerPhi);
        fHistJetNTowervsEta->Fill(towerEta);
        fHistJetNTowervsPhivsEta->Fill(towerPhi, towerEta);
        fHistQATowIDvsEta->Fill(towerID, towerEta);
        fHistQATowIDvsPhi->Fill(towerID, towerPhi);

        // increase tower counter
        nc++;
      } // towers

    } // uid < 0

  }  // end of constituent loop

  // set some jet properties
  jet->SetNumberOfTracks(nt);
  jet->SetNumberOfClusters(nc);
  jet->SetMaxTrackPt(maxTrack);
  jet->SetMaxClusterPt(maxTower);
  jet->SetMaxTowerE(maxTower);     // should this be Et? FIXME
  jet->SetNEF(neutralE/jet->E());  // should this be Et? FIXME
  //jet->SortConstituents(); // TODO see how this works - sorts ClusterIds() and TrackIds() by index (increasing)

  // fill jets histograms
  fHistNJetsvsPt->Fill(jet->Pt()); 
  fHistNJetsvsPhi->Fill(jet->Phi());
  fHistNJetsvsEta->Fill(jet->Eta());
  fHistNJetsvsPhivsEta->Fill(jet->Phi(), jet->Eta());
  fHistNJetsvsArea->Fill(jet->Area());  // same as fjw.GetJetArea(ij)
  fHistNJetsvsNConstituents->Fill(nt + nc);
  fHistNJetsvsNTracks->Fill(nt);
  fHistNJetsvsNTowers->Fill(nc);

}

/**
 * Sorts jets by pT (decreasing)
 * @param[out] indexes This array is used to return the indexes of the jets ordered by pT
 * @param[in] array Vector containing the list of jets obtained by the FastJet wrapper
 * @return kTRUE if at least one jet was found in array; kFALSE otherwise
 */
Bool_t StJetMakerTask::GetSortedArray(Int_t indexes[], std::vector<fastjet::PseudoJet> array) const
{
  static Float_t pt[9999] = {0};
  const Int_t n = (Int_t)array.size();
  if(n < 1) return kFALSE;

  for(Int_t i = 0; i < n; i++)
    pt[i] = array[i].perp();

  TMath::Sort(n, pt, indexes);

  return kTRUE;
}

/**
* An instance of this class can be "locked". Once locked, it cannot be unlocked.
 * If the instance is locked, attempting to change the configuration will throw a
 * fatal and stop the execution of the program. This method checks whether the instance
 * is locked and throw a fatal if it is locked.
 */
Bool_t StJetMakerTask::IsLocked() const
{
  if (fLocked) {
    Form("Jet finder task is locked! Changing properties is not allowed."); 
    return kStFatal;
  } 
  else {
    return kStOK;
  }
}

/**
 * Converts the internal enum values representing jet algorithms in
 * the corresponding values accepted by the FastJet wrapper.
 * @param algo Algorithm represented in the EJetAlgo_t enum
 * @return Algortithm represented in the fastjet::JetAlgorithm enum
 */
/*
fastjet::JetAlgorithm StJetMakerTask::ConvertToFJAlgo(EJetAlgo_t algo)
{
  switch(algo) {
  case StJetFrameworkPicoBase::kt_algorithm:
    return fastjet::kt_algorithm;
  case StJetFrameworkPicoBase::antikt_algorithm:
    return fastjet::antikt_algorithm;
  case StJetFrameworkPicoBase::cambridge_algorithm:
    return fastjet::cambridge_algorithm;
  case StJetFrameworkPicoBase::genkt_algorithm:
    return fastjet::genkt_algorithm;
  case StJetFrameworkPicoBase::cambridge_for_passive_algorithm:
    return fastjet::cambridge_for_passive_algorithm;
  case StJetFrameworkPicoBase::genkt_for_passive_algorithm:
    return fastjet::genkt_for_passive_algorithm;
  case StJetFrameworkPicoBase::plugin_algorithm:
    return fastjet::plugin_algorithm;
  case StJetFrameworkPicoBase::undefined_jet_algorithm:
    return fastjet::undefined_jet_algorithm;

  default:
    ::Error("StJetMakerTask::ConvertToFJAlgo", "Jet algorithm %d not recognized!!!", algo);
    return fastjet::undefined_jet_algorithm;
  }
}
*/

/**
 * Converts the internal enum values representing jet recombination schemes in
 * the corresponding values accepted by the FastJet wrapper.
 * @param reco Recombination scheme represented in the EJetAlgo_t enum
 * @return Recombination scheme represented in the fastjet::JetAlgorithm enum
 */
/*
fastjet::RecombinationScheme StJetMakerTask::ConvertToFJRecoScheme(ERecoScheme_t reco)
{
  switch(reco) {
  case StJetFrameworkPicoBase::E_scheme:
    return fastjet::E_scheme;
  case StJetFrameworkPicoBase::pt_scheme:
    return fastjet::pt_scheme;
  case StJetFrameworkPicoBase::pt2_scheme:
    return fastjet::pt2_scheme;
  case StJetFrameworkPicoBase::Et_scheme:
    return fastjet::Et_scheme;
  case StJetFrameworkPicoBase::Et2_scheme:
    return fastjet::Et2_scheme;
  case StJetFrameworkPicoBase::BIpt_scheme:
    return fastjet::BIpt_scheme;
  case StJetFrameworkPicoBase::BIpt2_scheme:
    return fastjet::BIpt2_scheme;
  case StJetFrameworkPicoBase::external_scheme:
    return fastjet::external_scheme;

  default:
    ::Error("StJetMakerTask::ConvertToFJRecoScheme", "Recombination scheme %d not recognized!!!", reco);
    return fastjet::external_scheme;
  }
}
*/

//________________________________________________________________________
Bool_t StJetMakerTask::AcceptJetTrack(StPicoTrack *trk, Float_t B, StThreeVectorF Vert) {
  // constants: assume neutral pion mass
  ///double pi0mass = Pico::mMass[0]; // GeV
  double pi = 1.0*TMath::Pi();

  // get momentum vector of track - global or primary track
  StThreeVectorF mTrkMom;
  if(doUsePrimTracks) { 
    if(!(trk->isPrimary())) return kFALSE; // check if primary
    // get primary track vector
    mTrkMom = trk->pMom(); 
  } else { 
    // get global track vector
    mTrkMom = trk->gMom(Vert, B); 
  }

  // track variables
  double pt = mTrkMom.perp();
  double phi = mTrkMom.phi();
  double eta = mTrkMom.pseudoRapidity();
  //double px = mTrkMom.x();
  //double py = mTrkMom.y();
  //double pz = mTrkMom.z();
  //double p = mTrkMom.mag();
  //double energy = 1.0*TMath::Sqrt(p*p + pi0mass*pi0mass);
  //short charge = trk->charge();
  double dca = (trk->dcaPoint() - mPicoEvent->primaryVertex()).mag();
  int nHitsFit = trk->nHitsFit();
  int nHitsMax = trk->nHitsMax();
  double nHitsRatio = 1.0*nHitsFit/nHitsMax;

  // pt cut
  if(pt < fMinJetTrackPt) return kFALSE;

  // jet track acceptance cuts now - after getting 3vector - hardcoded
  if(pt > fMaxJetTrackPt) return kFALSE; // 20.0 STAR, 100.0 ALICE
  if((eta < fJetTrackEtaMin) || (eta > fJetTrackEtaMax)) return kFALSE;
  if(phi < 0)    phi+= 2*pi;
  if(phi > 2*pi) phi-= 2*pi;
  if((phi < fJetTrackPhiMin) || (phi > fJetTrackPhiMax)) return kFALSE;
      
  // additional quality cuts for tracks
  if(dca > fJetTrackDCAcut)            return kFALSE;
  if(nHitsFit < fJetTracknHitsFit)     return kFALSE;
  if(nHitsRatio < fJetTracknHitsRatio) return kFALSE;

  // passed all above cuts - keep track and fill input vector to fastjet
  return kTRUE;
}

//
// Tower Quality Cuts
//________________________________________________________________________
Bool_t StJetMakerTask::AcceptJetTower(StPicoBTowHit *tower) {
  // get EMCal position
  StEmcPosition *mPosition = new StEmcPosition();

  // constants:
  double pi = 1.0*TMath::Pi();

  // tower ID
  int towerID = tower->id();

  // make sure some of these aren't still in event array
  if(towerID < 0) return kFALSE; 

  // cluster and tower position - from vertex and ID: shouldn't need additional eta correction
  StThreeVectorF towerPosition = mPosition->getPosFromVertex(mVertex, towerID);
  double phi = towerPosition.phi();
  if(phi < 0)    phi += 2.0*pi;
  if(phi > 2*pi) phi -= 2.0*pi;
  double eta = towerPosition.pseudoRapidity();

  // check for bad (and dead) towers
  bool TowerOK = IsTowerOK(towerID);      // kTRUE means GOOD
  bool TowerDead = IsTowerDead(towerID);  // kTRUE means BAD
  if(!TowerOK)  { return kFALSE; }
  if(TowerDead) { return kFALSE; }

  // jet track acceptance cuts now - after getting 3vector - hardcoded
  if((eta < fJetTowerEtaMin) || (eta > fJetTowerEtaMax)) return kFALSE;
  if(phi < 0)    phi+= 2*pi;
  if(phi > 2*pi) phi-= 2*pi;
  if((phi < fJetTowerPhiMin) || (phi > fJetTowerPhiMax)) return kFALSE;

  // passed all above cuts - keep tower and fill input vector to fastjet
  return kTRUE;
}

//________________________________________________________________________
Int_t StJetMakerTask::GetCentBin(Int_t cent, Int_t nBin) const
{  // Get centrality bin.
  Int_t centbin = -1;

  if(nBin == 16) { centbin = nBin - 1 - cent; }
  if(nBin == 9)  { centbin = nBin - 1 - cent; }

  return centbin;
}

//__________________________________________________________________________________________
Bool_t StJetMakerTask::SelectAnalysisCentralityBin(Int_t centbin, Int_t fCentralitySelectionCut) {
  // this function is written to cut on centrality in a task for a given range
  // STAR centrality is written in re-verse binning (0,15) where lowest bin is highest centrality
  // -- this is a very poor approach
  // -- Solution, Use:  Int_t StJetFrameworkPicoBase::GetCentBin(Int_t cent, Int_t nBin) const
  //    in order remap the centrality to a more appropriate binning scheme
  //    (0, 15) where 0 will correspond to the 0-5% bin
  // -- NOTE: Use the 16 bin scheme instead of 9, its more flexible
  // -- Example usage:
  //    Int_t cent16 = grefmultCorr->getCentralityBin16();
  //    Int_t centbin = GetCentBin(cent16, 16);
  //    if(!SelectAnalysisCentralityBin(centbin, StJetFrameworkPicoBase::kCent3050)) return kStWarn; (or StOk to suppress warnings)
  //
  // other bins can be added if needed...

  Bool_t doAnalysis = kFALSE; // set false by default, to make sure user chooses an available bin

  // switch on bin selection
  switch(fCentralitySelectionCut) {
    case StJetFrameworkPicoBase::kCent010 :  // 0-10%
      if((centbin>-1) && (centbin<2)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent020 :  // 0-20%
      if((centbin>-1) && (centbin<4)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent1020 : // 10-20%
      if((centbin>1) && (centbin<4)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent1030 : // 10-30%
      if((centbin>1) && (centbin<6)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent1040 : // 10-40%
      if((centbin>1) && (centbin<8)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent2030 : // 20-30%
      if((centbin>3) && (centbin<6)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent2040 : // 20-40%
      if((centbin>3) && (centbin<8)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent2050 : // 20-50%
      if((centbin>3) && (centbin<10)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent2060 : // 20-60%
      if((centbin>3) && (centbin<12)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent3050 : // 30-50%
      if((centbin>5) && (centbin<10)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent3060 : // 30-60%
      if((centbin>5) && (centbin<12)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent4060 : // 40-60%
      if((centbin>7) && (centbin<12)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent4070 : // 40-70%
      if((centbin>7) && (centbin<14)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent4080 : // 40-80%
      if((centbin>7) && (centbin<16)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent5080 : // 50-80%
      if((centbin>9) && (centbin<16)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    case StJetFrameworkPicoBase::kCent6080 : // 60-80%
      if((centbin>11) && (centbin<16)) { doAnalysis = kTRUE; }
      else { doAnalysis = kFALSE; }
      break;

    default : // wrong entry
      doAnalysis = kFALSE;

  }

  return doAnalysis;
}

//________________________________________________________________________________________________________
Bool_t StJetMakerTask::GetMomentum(StThreeVectorF &mom, const StPicoBTowHit* tower, Double_t mass) const {
  // initialize Emc position objects
  StEmcPosition *Position = new StEmcPosition();

  // vertex components - only need if below method is used
  // mGeom3->getEtaPhi(towerID,tEta,tPhi);
  //double xVtx = mVertex.x();
  //double yVtx = mVertex.y();
  //double zVtx = mVertex.z();

  // get mass, E, P, ID
  if(mass < 0) mass = 0;
  Double_t energy = tower->energy();
  Double_t p = TMath::Sqrt(energy*energy - mass*mass);
  int towerID = tower->id();

  // get tower position
  StThreeVectorF towerPosition = Position->getPosFromVertex(mVertex, towerID);
  double posX = towerPosition.x();
  double posY = towerPosition.y();
  double posZ = towerPosition.z();

  // shouldn't need correction with above method
//  posX-=xVtx;
//  posY-=yVtx;
//  posZ-=zVtx;

  // get r, set position components
  Double_t r = TMath::Sqrt(posX*posX + posY*posY + posZ*posZ) ;
  if(r > 1e-12) {
    mom.setX( p*posX/r );
    mom.setY( p*posY/r );
    mom.setZ( p*posZ/r );
  } else { return kFALSE; }
  return kTRUE;
}

//____________________________________________________________________________________________
Bool_t StJetMakerTask::IsTowerOK( Int_t mTowId ){
  //if( badTowers.size()==0 ){
  if( badTowers.empty() ){
    __ERROR("StJetMakerTask::IsTowerOK: WARNING: You're trying to run without a bad tower list. If you know what you're doing, deactivate this throw and recompile.");
    throw ( -1 );
  }
  if( badTowers.count( mTowId )>0 ){
    __DEBUG(9, Form("Reject. Tower ID: %d", mTowId));
    return kFALSE;
  } else {
    __DEBUG(9, Form("Accept. Tower ID: %d", mTowId));
    return kTRUE;
  }    
}

//____________________________________________________________________________________________
Bool_t StJetMakerTask::IsTowerDead( Int_t mTowId ){
  //if( deadTowers.size()==0 ){
  if( deadTowers.empty() ){
    __ERROR("StJetMakerTask::IsTowerDead: WARNING: You're trying to run without a dead tower list. If you know what you're doing, deactivate this throw and recompile.");
    throw ( -1 );
  }
  if( deadTowers.count( mTowId )>0 ){
    __DEBUG(9, Form("Reject. Tower ID: %d", mTowId));
    return kTRUE;
  } else {
    __DEBUG(9, Form("Accept. Tower ID: %d", mTowId));
    return kFALSE;
  }
}

//____________________________________________________________________________
void StJetMakerTask::ResetBadTowerList( ){
  badTowers.clear();
}

// Add bad towers from comma separated values file
// Can be split into arbitrary many lines
// Lines starting with # will be ignored
Bool_t StJetMakerTask::AddBadTowers(TString csvfile){  
  // open infile
  std::string line;
  std::ifstream inFile ( csvfile );

  __DEBUG(2, Form("Loading bad towers from %s", csvfile.Data()) );
	  
  if( !inFile.good() ) {
    __WARNING(Form("Can't open %s", csvfile.Data()) );
    return kFALSE;
  }
  
  while(std::getline (inFile, line) ){
    if( line.size()==0 ) continue; // skip empty lines
    if( line[0] == '#' ) continue; // skip comments

    std::istringstream ss( line );
    while( ss ){
      std::string entry;
      std::getline( ss, entry, ',' );
      int ientry = atoi(entry.c_str());
      if(ientry) {
	badTowers.insert( ientry );
	__DEBUG(2, Form("Added bad tower # %d", ientry));
      }
    }
  }
  
  return kTRUE;
}

// Add dead towers from comma separated values file
// Can be split into arbitrary many lines
// Lines starting with # will be ignored
Bool_t StJetMakerTask::AddDeadTowers(TString csvfile){
  // open infile
  std::string line;
  std::ifstream inFile ( csvfile );

  __DEBUG(2, Form("Loading bad towers from %s", csvfile.Data()) );

  if( !inFile.good() ) {
    __WARNING(Form("Can't open %s", csvfile.Data()) );
    return kFALSE;
  }

  while(std::getline (inFile, line) ){
    if( line.size()==0 ) continue; // skip empty lines
    if( line[0] == '#' ) continue; // skip comments

    std::istringstream ss( line );
    while( ss ){
      std::string entry;
      std::getline( ss, entry, ',' );
      int ientry = atoi(entry.c_str());
      if(ientry) {
        deadTowers.insert( ientry );
        __DEBUG(2, Form("Added bad tower # %d", ientry));
      }
    }
  }

  return kTRUE;
}

//____________________________________________________________________________
void StJetMakerTask::ResetDeadTowerList( ){
  deadTowers.clear();
}

//_________________________________________________________________________
Bool_t StJetMakerTask::CheckForMB(int RunFlag, int type) {
  // Run14 triggers:
  int arrMB_Run14[] = {450014};
  int arrMB30_Run14[] = {450010, 450020};
  int arrMB5_Run14[] = {450005, 450008, 450009, 450014, 450015, 450018, 450024, 450025, 450050, 450060};
  // additional 30: 4, 5, 450201, 450202, 450211, 450212
  // 1: VPDMB-5   Run:            15075055 - 15076099
  // 1: VPDMB-5-p-nobsmd-hlt Run: 15081020 - 15090048
  // 4: VPDMB-5-p-nobsmd-hlt Run: 15090049 - 15167007

  // Run16 triggers:
  int arrMB_Run16[] = {520021};
  int arrMB5_Run16[] = {520001, 520002, 520003, 520011, 520012, 520013, 520021, 520022, 520023, 520031, 520033, 520041, 520042, 520043, 520051, 520822, 520832, 520842, 570702};
  int arrMB10_Run16[] = {520007, 520017, 520027, 520037, 520201, 520211, 520221, 520231, 520241, 520251, 520261, 520601, 520611, 520621, 520631, 520641};

  // run flag selection to check for MB firing
  switch(RunFlag) {
    case StJetFrameworkPicoBase::Run14_AuAu200 : // Run14 AuAu
        switch(type) { 
          case StJetFrameworkPicoBase::kRun14main :
              if((DoComparison(arrMB_Run14, sizeof(arrMB_Run14)/sizeof(*arrMB_Run14)))) { return kTRUE; }
              break;
          case StJetFrameworkPicoBase::kVPDMB5 :
              if((DoComparison(arrMB5_Run14, sizeof(arrMB5_Run14)/sizeof(*arrMB5_Run14)))) { return kTRUE; }
              break;
          case StJetFrameworkPicoBase::kVPDMB30 :
              if((DoComparison(arrMB30_Run14, sizeof(arrMB30_Run14)/sizeof(*arrMB30_Run14)))) { return kTRUE; }
              break;
          default :
              if((DoComparison(arrMB_Run14, sizeof(arrMB_Run14)/sizeof(*arrMB_Run14)))) { return kTRUE; }
        }
        break; // added May20

    case StJetFrameworkPicoBase::Run16_AuAu200 : // Run16 AuAu
        switch(type) {
          case StJetFrameworkPicoBase::kRun16main :
              if((DoComparison(arrMB_Run16, sizeof(arrMB_Run16)/sizeof(*arrMB_Run16)))) { return kTRUE; }
              break;
          case StJetFrameworkPicoBase::kVPDMB5 :
              if((DoComparison(arrMB5_Run16, sizeof(arrMB5_Run16)/sizeof(*arrMB5_Run16)))) { return kTRUE; }
              break;
          case StJetFrameworkPicoBase::kVPDMB10 :
              if((DoComparison(arrMB10_Run16, sizeof(arrMB10_Run16)/sizeof(*arrMB10_Run16)))) { return kTRUE; }
              break;
          default :
              if((DoComparison(arrMB_Run16, sizeof(arrMB_Run16)/sizeof(*arrMB_Run16)))) { return kTRUE; }
        }
        break; // added May20

  } // RunFlag switch

  // return status
  return kFALSE;
} // MB function

//
// check to see if the event was EMC triggered for High Towers
//____________________________________________________________________________
Bool_t StJetMakerTask::CheckForHT(int RunFlag, int type) {

  // Run14 triggers:
  int arrHT1_Run14[] = {450201, 450211, 460201};
  int arrHT2_Run14[] = {450202, 450212, 460202, 460212};
  int arrHT3_Run14[] = {450203, 450213, 460203};

  // Run16 triggers:
  int arrHT1_Run16[] = {520201, 520211, 520221, 520231, 520241, 520251, 520261, 520605, 520615, 520625, 520635, 520645, 520655, 550201, 560201, 560202, 530201, 540201};
  int arrHT2_Run16[] = {530202, 540203};
  int arrHT3_Run16[] = {520203, 530213};

  // run flag selection to check for MB firing
  switch(RunFlag) {
    case StJetFrameworkPicoBase::Run14_AuAu200 : // Run14 AuAu
        switch(type) {
          case StJetFrameworkPicoBase::kIsHT1 :
              if((DoComparison(arrHT1_Run14, sizeof(arrHT1_Run14)/sizeof(*arrHT1_Run14)))) { return kTRUE; }
              break;
          case StJetFrameworkPicoBase::kIsHT2 :
              if((DoComparison(arrHT2_Run14, sizeof(arrHT2_Run14)/sizeof(*arrHT2_Run14)))) { return kTRUE; }
              break;
          case StJetFrameworkPicoBase::kIsHT3 :
              if((DoComparison(arrHT3_Run14, sizeof(arrHT3_Run14)/sizeof(*arrHT3_Run14)))) { return kTRUE; }
              break;
          default :  // default to HT2
              if((DoComparison(arrHT2_Run14, sizeof(arrHT2_Run14)/sizeof(*arrHT2_Run14)))) { return kTRUE; }
        }
        break; // added May20

    case StJetFrameworkPicoBase::Run16_AuAu200 : // Run16 AuAu
        switch(type) {
          case StJetFrameworkPicoBase::kIsHT1 :
              if((DoComparison(arrHT1_Run16, sizeof(arrHT1_Run16)/sizeof(*arrHT1_Run16)))) { return kTRUE; }
              break;
          case StJetFrameworkPicoBase::kIsHT2 :
              if((DoComparison(arrHT2_Run16, sizeof(arrHT2_Run16)/sizeof(*arrHT2_Run16)))) { return kTRUE; }
              break;
          case StJetFrameworkPicoBase::kIsHT3 :
              if((DoComparison(arrHT3_Run16, sizeof(arrHT3_Run16)/sizeof(*arrHT3_Run16)))) { return kTRUE; }
              break;
          default :  // Run16 only has HT1's
              if((DoComparison(arrHT1_Run16, sizeof(arrHT1_Run16)/sizeof(*arrHT1_Run16)))) { return kTRUE; }
        }
        break; // added May20

  } // RunFlag switch

  return kFALSE;
}

//________________________________________________________________________
Bool_t StJetMakerTask::DoComparison(int myarr[], int elems) {
  //std::cout << "Length of array = " << (sizeof(myarr)/sizeof(*myarr)) << std::endl;
  bool match = kFALSE;

  // loop over specific physics selection array and compare to specific event trigger
  for(int i = 0; i < elems; i++) {
    if(mPicoEvent->isTrigger(myarr[i])) match = kTRUE;
    if(match) break;
  }
  //cout<<"elems: "<<elems<<"  match: "<<match<<endl;

  return match;
}

//_________________________________________________________________________
void StJetMakerTask::FillEmcTriggersArr() {
  // zero out trigger array and get number of Emcal Triggers
  for(int i = 0; i < 8; i++) { fEmcTriggerArr[i] = kFALSE; }
  int nEmcTrigger = mPicoDst->numberOfEmcTriggers();

  // tower - HT trigger types array
  // zero these out - so they are refreshed for each event
  for(int i = 0; i < 4801; i++) {
    fTowerToTriggerTypeHT1[i] = kFALSE;
    fTowerToTriggerTypeHT2[i] = kFALSE;
    fTowerToTriggerTypeHT3[i] = kFALSE;
  }

  // loop over valid EmcalTriggers
  for(int i = 0; i < nEmcTrigger; i++) {
    StPicoEmcTrigger *emcTrig = static_cast<StPicoEmcTrigger*>(mPicoDst->emcTrigger(i));
    if(!emcTrig) continue;

    // emc trigger parameters
    int emcTrigID = emcTrig->id();

    // check if i'th trigger fired HT triggers by meeting threshold
    bool isHT1 = emcTrig->isHT1();
    bool isHT2 = emcTrig->isHT2();
    bool isHT3 = emcTrig->isHT3();
    if(isHT1) fTowerToTriggerTypeHT1[emcTrigID] = kTRUE;
    if(isHT2) fTowerToTriggerTypeHT2[emcTrigID] = kTRUE;
    if(isHT3) fTowerToTriggerTypeHT3[emcTrigID] = kTRUE;

    // fill for valid triggers
    if(isHT1) fEmcTriggerArr[StJetFrameworkPicoBase::kIsHT1] = kTRUE;
    if(isHT2) fEmcTriggerArr[StJetFrameworkPicoBase::kIsHT2] = kTRUE;
    if(isHT3) fEmcTriggerArr[StJetFrameworkPicoBase::kIsHT3] = kTRUE;
  }

}
//========
/*
      // get tower location - from ID: need to correct eta (when using this method)
      float tEta, tPhi;    // need to be floats
      StEmcGeom *mGeom3 = (StEmcGeom::instance("bemc"));
      double radius = mGeom3->Radius();
      mGeom3->getEtaPhi(towerID,tEta,tPhi);
      if(tPhi < 0)    tPhi += 2*pi;
      if(tPhi > 2*pi) tPhi -= 2*pi;

     // correct eta for Vz position 
     double theta;
     theta = 2 * atan(exp(-tEta)); // getting theta from eta 
     double z = 0;
     if(tEta != 0) z = radius / tan(theta);  // 231 cm = radius of SMD 
     double zNominal = z - mVertex.z();
     double thetaCorr = atan2(radius, zNominal); // theta with respect to primary vertex
     double etaCorr =-log(tan(thetaCorr / 2.0)); // eta with respect to primary vertex 

     cout<<"tEta = "<<tEta<<"  etaCorr = "<<etaCorr<<"  towerEta = "<<towerEta<<"  tPhi = "<<tPhi<<"  towerPhi = "<<towerPhi<<endl;
*/
//==========

