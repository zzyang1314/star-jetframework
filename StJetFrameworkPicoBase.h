#ifndef StMyAnalysisMaker_h
#define StMyAnalysisMaker_h

// some includes
#include "StMaker.h"
#include "StRoot/StPicoEvent/StPicoEvent.h"

// ROOT classes
class TClonesArray;
class TF1;
class TH1;
class TH1F;
class TH2;
class TH2F;
class TH3;
class THnSparse;
class TString;

// STAR classes
class StPicoDst;
class StPicoDstMaker;
class StPicoEvent;
class StPicoTrack;
class StRefMultCorr;

// my STAR classes
class StJetMakerTask;
class StJet;
class StRho;
class StRhoParameter;
class StEventPoolManager;

class StMyAnalysisMaker : public StMaker {
  public:

  // jet type enumerator
  enum EJetType_t {
    kFullJet,
    kChargedJet,
    kNeutralJet
  };

  // jet algorithm enumerator
  enum EJetAlgo_t {
    kt_algorithm                    = 0,
    antikt_algorithm                = 1,
    cambridge_algorithm             = 2,
    genkt_algorithm                 = 3,
    cambridge_for_passive_algorithm = 11,
    genkt_for_passive_algorithm     = 13,
    plugin_algorithm                = 99,
    undefined_jet_algorithm         = 999
  };

  // jet recombination schme enumerator
  enum ERecoScheme_t {
    E_scheme        = 0,
    pt_scheme       = 1,
    pt2_scheme      = 2,
    Et_scheme       = 3,
    Et2_scheme      = 4,
    BIpt_scheme     = 5,
    BIpt2_scheme    = 6,
    WTA_pt_scheme   = 7,
    WTA_modp_scheme = 8,
    external_scheme = 99
  };

    // event plane track weight type enumerator
    enum EPtrackWeigthType_t {
      kNoWeight,
      kPtLinearWeight,
      kPtLinear2Const5Weight
    };

    StMyAnalysisMaker(const char *name, StPicoDstMaker *picoMaker, const char *outName, bool mDoComments, double minJetPtCut, double trkbias, const char *jetMakerName, const char *rhoMakerName);
    virtual ~StMyAnalysisMaker();
   
    // class required functions
    virtual Int_t Init();
    virtual Int_t Make();
    virtual void  Clear(Option_t *opt="");
    virtual Int_t Finish();
    
    // booking of histograms (optional)
    void    DeclareHistograms();
    void    WriteHistograms();
   
    // THnSparse Setup
    virtual THnSparse*      NewTHnSparseF(const char* name, UInt_t entries);
    virtual void            GetDimParams(Int_t iEntry,TString &label, Int_t &nbins, Double_t &xmin, Double_t &xmax);
    virtual THnSparse*      NewTHnSparseFCorr(const char* name, UInt_t entries);
    virtual void            GetDimParamsCorr(Int_t iEntry,TString &label, Int_t &nbins, Double_t &xmin, Double_t &xmax);

    static TString GenerateJetName(EJetType_t jetType, EJetAlgo_t jetAlgo, ERecoScheme_t recoScheme, Double_t radius, TClonesArray* partCont, TClonesArray* clusCont, TString tag);

    // TClonesArrays function returners of analysis objects
    TClonesArray* jets() const { return mJets; }
    TClonesArray* tracks() const { return mTracks; }
    TClonesArray* towers() const { return mTowers; }
    TClonesArray* particles() const { return mParticles; }

    // switches
    virtual void            SetUsePrimaryTracks(Bool_t P)      { doUsePrimTracks   = P; }

    // jet setters
    virtual void            SetMinJetPt(Double_t j)            { fMinPtJet         = j; }    // min jet pt
    virtual void            SetJetMaxTrackPt(Double_t t)       { fTrackBias        = t; }    // track bias
    virtual void            SetJetRad(Double_t jrad)           { fJetRad           = jrad; } // jet radius 
    
    // event setters
    virtual void            SetEventZVtxRange(Double_t zmi, Double_t zma) { fEventZVtxMinCut = zmi; fEventZVtxMaxCut = zma; }

    // track setters
    virtual void            SetMinTrackPt(Double_t minpt)      { fTrackPtMinCut    = minpt;} // min track cut
    virtual void            SetMaxTrackPt(Double_t maxpt)      { fTrackPtMaxCut    = maxpt;} // max track cut
    virtual void            SetTrackPhiRange(Double_t ptmi, Double_t ptma) { fTrackPhiMinCut = ptmi; fTrackPhiMaxCut = ptma; }
    virtual void            SetTrackEtaRange(Double_t etmi, Double_t etma) { fTrackEtaMinCut = etmi; fTrackEtaMaxCut = etma; }
    virtual void            SetTracknHitsFit(Double_t h)       { fTracknHitsFit = h     ; }
    virtual void            SetTracknHitsRatio(Double_t r)     { fTracknHitsRatio = r   ; }

    // event mixing - setters
    virtual void            SetEventMixing(Int_t yesno)	       { fDoEventMixing=yesno; }
    virtual void            SetMixingTracks(Int_t tracks)      { fMixingTracks = tracks; }
    virtual void            SetNMixedTr(Int_t nmt)             { fNMIXtracks = nmt; }
    virtual void            SetNMixedEvt(Int_t nme)            { fNMIXevents = nme; }

    // mixed selection - setters
    virtual void            SetTriggerEventType(UInt_t te)       { fTriggerEventType = te; }
    virtual void            SetMixedEventType(UInt_t me)         { fMixingEventType = me; }
    virtual void            SetCentBinSize(Int_t centbins)       { fCentBinSize = centbins; }
    virtual void            SetReduceStatsCent(Int_t red)        { fReduceStatsCent = red; }

    // efficiency correction setter
    virtual void            SetDoEffCorr(Int_t effcorr)          { fDoEffCorr = effcorr; }

    // use rho to correct jet pt in correlation sparses
    virtual void            SetCorrectJetPt(Bool_t cpt)          { fCorrJetPt = cpt; }

    // event plane
    StJet*                  GetLeadingJet(StRhoParameter* eventRho = 0x0);
    virtual void            SetExcludeLeadingJetsFromFit(Float_t n)         {fExcludeLeadingJetsFromFit = n; }
    virtual void            SetEventPlaneTrackWeight(int weight)            {fTrackWeight = weight; }

  protected:
    Int_t                  GetCentBin(Int_t cent, Int_t nBin) const; // centrality bin
    Double_t               RelativePhi(Double_t mphi,Double_t vphi) const; // relative jet track angle
    Double_t               RelativeEPJET(Double_t jetAng, Double_t EPAng) const;  // relative jet event plane angle
    TH1*                   FillEventTriggerQA(TH1* h, UInt_t t); // filled event trigger QA plots
    Double_t               GetReactionPlane(); // get reaction plane angle
    Bool_t                 AcceptTrack(StPicoTrack *trk, Float_t B, StThreeVectorF Vert);  // track accept cuts function

    //Double_t               EffCorrection(Double_t trkETA, Double_t trkPT, Int_t effswitch) const; // efficiency correction function

    // switches
    Bool_t                 doUsePrimTracks;         // primary track switch
    Int_t                  fDoEffCorr; // efficiency correction to tracks
    Bool_t                 fCorrJetPt; // correct jet pt by rho

    // cuts
    Double_t               fMinPtJet;               // min jet pt to keep jet in output
    Double_t               fTrackBias;              // high pt track in jet bias
    Double_t               fJetRad;                 // jet radius
    Double_t               fEventZVtxMinCut;        // min event z-vertex cut
    Double_t               fEventZVtxMaxCut;        // max event z-vertex cut
    Double_t               fTrackPtMinCut;          // min track pt cut
    Double_t               fTrackPtMaxCut;          // max track pt cut
    Double_t               fTrackPhiMinCut;         // min track phi cut
    Double_t               fTrackPhiMaxCut;         // max track phi cut
    Double_t               fTrackEtaMinCut;         // min track eta cut
    Double_t               fTrackEtaMaxCut;         // max track eta cut
    Int_t                  fTracknHitsFit;       // requirement for track hits
    Double_t               fTracknHitsRatio;     // requirement for nHitsFit / nHitsMax


    // event mixing
    Int_t          fDoEventMixing;
    Int_t          fMixingTracks;
    Int_t          fNMIXtracks;
    Int_t          fNMIXevents;
    Int_t          fCentBinSize; // centrality bin size of mixed event pools
    Int_t          fReduceStatsCent; // bins to use for reduced statistics of sparse

    // event selection types
    UInt_t         fTriggerEventType;
    UInt_t         fMixingEventType;

    // used for event plane calculation and resolution
    StJet*         fLeadingJet;//! leading jet
    Float_t        fExcludeLeadingJetsFromFit;    // exclude n leading jets from fit
    Int_t          fTrackWeight; // track weight for Q-vector summation

    // event pool
    TClonesArray      *CloneAndReduceTrackList(TClonesArray* tracks);
    StEventPoolManager   *fPoolMgr;//!  // event pool Manager object

    // clonesarray collections of tracks and jets
    TClonesArray          *fTracksME;//! track collection to slim down for mixed events
    TClonesArray          *fJets;//! jet collection

  private:
    // PicoDstMaker and PicoDst object pointer
    StPicoDstMaker *mPicoDstMaker;
    StPicoDst      *mPicoDst;
    StPicoEvent    *mPicoEvent;
    StJetMakerTask   *JetMaker;
    StRho          *RhoMaker;

    // centrality objects
    StRefMultCorr* grefmultCorr;
    StRefMultCorr* refmultCorr;
    StRefMultCorr* refmult2Corr;

    // TCloneArray of analysis objects
    TClonesArray   *mJets;
    TClonesArray   *mTracks;
    TClonesArray   *mTowers;
    TClonesArray   *mParticles;
   
    // output file name string 
    TString      mOutName;
 
    // counters 
    Int_t        mEventCounter;//!
    Int_t        mAllPVEventCounter;//!
    Int_t        mInputEventCounter;//!
 
    // switches
    bool         doComments;

    // histograms
    TH1F* hTriggerPt;//!
    TH1F* hEventPlane;//!   
 
    // jet histos
    TH1F* hJetPt;//!
    TH1F* hJetCorrPt;//!
    TH1F* hJetPt2;//!
    TH1F* hJetE;//!
    TH1F* hJetEta;//!
    TH1F* hJetPhi;//!
    TH1F* hJetNEF;//!
    TH1F* hJetArea;//!
    TH1F* hJetTracksPt;//!
    TH1F* hJetTracksPhi;//!
    TH1F* hJetTracksEta;//!

    // correlation histo
    TH2  *fHistJetHEtaPhi;//!

    // QA histos
    TH1  *fHistEventSelectionQA;//! 
    TH1  *fHistEventSelectionQAafterCuts;//!
    TH1  *hTriggerIds;//!

    // THn Sparse's jet sparse
    THnSparse             *fhnJH;//!           // jet hadron events matrix
    THnSparse             *fhnMixedEvents;//!  // mixed events matrix
    THnSparse             *fhnCorr;//!         // sparse to get # jet triggers

    // Rho objects
    StRhoParameter        *GetRhoFromEvent(const char *name);
    StRhoParameter        *fRho;//!<!          // event rho
    Double_t               fRhoVal;//!<!       // event rho value, same for local rho
    TString                fRhoName;///<       // rho name

    // maker names
    TString                fAnalysisMakerName;
    TString                fJetMakerName;
    TString                fRhoMakerName;
    TString                fEventMixerMakerName;

    // counters
    Int_t GetEventCounter() {return mEventCounter;}
    Int_t GetAllPVEventCounter() {return mAllPVEventCounter;}
    Int_t GetInputEventCounter() {return mInputEventCounter;}
                
    ClassDef(StMyAnalysisMaker, 1)
};

#endif