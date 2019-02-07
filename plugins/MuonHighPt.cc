// system include files
#include <memory>
#include <cmath>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/ESHandle.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "DataFormats/Common/interface/ValueMap.h"
#include <DataFormats/MuonReco/interface/Muon.h>

#include "DataFormats/CLHEP/interface/AlgebraicObjects.h"
#include "DataFormats/GeometryCommonDetAlgo/interface/Measurement1D.h"
#include "DataFormats/TrackReco/interface/TrackBase.h"
#include "DataFormats/BeamSpot/interface/BeamSpot.h"

#include "TrackingTools/IPTools/interface/IPTools.h"
#include "TrackingTools/Records/interface/TransientTrackRecord.h"
#include "TrackingTools/TransientTrack/interface/TransientTrackBuilder.h"


//
// class declaration
//


bool isNewHighPtMuon(const reco::Muon& muon, const reco::Vertex& vtx){
  if(!muon.isGlobalMuon()) return false;

  bool muValHits = ( muon.globalTrack()->hitPattern().numberOfValidMuonHits()>0 ||
                     muon.tunePMuonBestTrack()->hitPattern().numberOfValidMuonHits()>0 );

  bool muMatchedSt = muon.numberOfMatchedStations()>1;
  if(!muMatchedSt) {
    if( muon.isTrackerMuon() && muon.numberOfMatchedStations()==1 ) {
      if( muon.expectedNnumberOfMatchedStations()<2 ||
          !(muon.stationMask()==1 || muon.stationMask()==16) ||
          muon.numberOfMatchedRPCLayers()>2
	  )
        muMatchedSt = true;
    }
  }

  bool muID = muValHits && muMatchedSt;

  bool hits = muon.innerTrack()->hitPattern().trackerLayersWithMeasurement() > 5 &&
    muon.innerTrack()->hitPattern().numberOfValidPixelHits() > 0; 

  bool momQuality = muon.tunePMuonBestTrack()->ptError()/muon.tunePMuonBestTrack()->pt() < 0.3;

  bool ip = fabs(muon.innerTrack()->dxy(vtx.position())) < 0.2 && fabs(muon.innerTrack()->dz(vtx.position())) < 0.5;

  return muID && hits && momQuality && ip;

}

class MuonHighPt : public edm::EDProducer {
public:
  explicit MuonHighPt(const edm::ParameterSet&);
  ~MuonHighPt();

private:
  virtual void beginJob() ;
  virtual void produce(edm::Event&, const edm::EventSetup&);
  virtual void endJob() ;

  // ----------member data ---------------------------
  const edm::EDGetTokenT<edm::View<reco::Muon>> probes_;    
  const edm::EDGetTokenT<std::vector<reco::Vertex>> vertexes_;    
  const edm::EDGetTokenT<reco::BeamSpot> bs_;    


};

//
// constants, enums and typedefs
//


//
// static data member definitions
//

//
// constructors and destructor
//
MuonHighPt::MuonHighPt(const edm::ParameterSet& iConfig):
probes_(consumes<edm::View<reco::Muon>>(iConfig.getParameter<edm::InputTag>("probes"))),
vertexes_(consumes<std::vector<reco::Vertex>>(edm::InputTag("offlinePrimaryVertices"))),
bs_(consumes<reco::BeamSpot>(edm::InputTag("offlineBeamSpot")))
{
  produces<edm::ValueMap<float> >("highPtIDNew");

}


MuonHighPt::~MuonHighPt()
{

  // do anything here that needs to be done at desctruction time
  // (e.g. close files, deallocate resources etc.)

}


//
// member functions
//

// ------------ method called to produce the data  ------------
void
MuonHighPt::produce(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
  using namespace edm;

  // read input
  Handle<View<reco::Muon> > probes;
  iEvent.getByToken(probes_,  probes);
  
  edm::Handle<std::vector<reco::Vertex> > vertex;
  iEvent.getByToken(vertexes_, vertex);
      
  edm::Handle<reco::BeamSpot> recoBeamSpotHandle;
  iEvent.getByToken(bs_,recoBeamSpotHandle);

  // prepare vector for output    
  std::vector<float> muon_highPtIDNew;

  // fill
  View<reco::Muon>::const_iterator probe, endprobes = probes->end();


  // getting the good primary vertex
  reco::Vertex::Point posVtx;
  reco::Vertex::Error errVtx;
  unsigned int theIndexOfThePrimaryVertex = 999.;
  if (!vertex.isValid()) {
    for (unsigned int ind=0; ind<vertex->size(); ++ind) {
      if ( (*vertex)[ind].isValid() && !((*vertex)[ind].isFake()) ) {
	theIndexOfThePrimaryVertex = ind;
	break;
      }
    }
  }

  if (theIndexOfThePrimaryVertex<100) {
    posVtx = ((*vertex)[theIndexOfThePrimaryVertex]).position();
    errVtx = ((*vertex)[theIndexOfThePrimaryVertex]).error();
  }   
  else {
    reco::BeamSpot bs = *recoBeamSpotHandle;
 
    posVtx = bs.position();
    errVtx(0,0) = bs.BeamWidthX();
    errVtx(1,1) = bs.BeamWidthY();
    errVtx(2,2) = bs.sigmaZ();
  }

  const reco::Vertex thePrimaryVertex(posVtx,errVtx);


  // loop on PROBES
  for (probe = probes->begin(); probe != endprobes; ++probe) {
    muon_highPtIDNew.push_back( isNewHighPtMuon(*probe, thePrimaryVertex) ) ;
  }// end loop on probes

  // convert into ValueMap and store
  std::unique_ptr<ValueMap<float> > highPtIDNew(new ValueMap<float>());
  ValueMap<float>::Filler filler0(*highPtIDNew);
  filler0.insert(probes, muon_highPtIDNew.begin(), muon_highPtIDNew.end());
  filler0.fill();

  iEvent.put(std::move(highPtIDNew), "highPtIDNew");


}

// ------------ method called once each job just before starting event loop  ------------
void 
MuonHighPt::beginJob()
{


}

// ------------ method called once each job just after ending the event loop  ------------
void 
MuonHighPt::endJob() {
}

//define this as a plug-in
DEFINE_FWK_MODULE(MuonHighPt);
