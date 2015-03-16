#include "TStarJetPicoReader.h"

#include <TChain.h>
#include <TList.h>
#include <TStopwatch.h>

#include "TStarJetPicoEventHeader.h"
#include "TStarJetPicoTower.h"

#include "TStarJetPicoEvent.h"
#include "TStarJetPicoEventCuts.h"
#include "TStarJetPicoPrimaryTrack.h"
#include "TStarJetPicoTower.h"
#include "TStarJetPicoV0.h"
#include "TStarJetPicoV0Cuts.h"
#include "TStarJetPicoTriggerInfo.h"
#include "TStarJetPicoTrackCuts.h"
#include "TStarJetPicoTowerCuts.h"

#include "TStarJetPicoUtils.h"
#include "TStarJetPicoDefinitions.h"
#include "TStarJetVectorContainer.h"

/////////////////////////////////////////////////////////////////////////
//                                                                     //
// TStarJetPicoReader                                                  //
//                                                                     //
// From specified TChain or directory of picoDST files                 //
// read TStarJetPicoEvent and apply cuts:                              //
//   TStarJetPicoEventCuts, TStarJetPicoTrackCuts,                     // 
//   TStarJetPicoTowerCuts, TStarJetPicoV0Cuts.                        //
// and fill the output vector fOutputContainer:                        //
//   TStarJetVectorContainer<TStarJetVector>.                          // 
// One can retrieve the selected (original survivors of the cuts) from TList(s): //
//   fSelectedTracks, fSelectedTowers, fSelectedV0s.                   //
//                                                                     //
/////////////////////////////////////////////////////////////////////////

ClassImp(TStarJetPicoReader)

TStarJetPicoReader::TStarJetPicoReader() 
  : TStarJetPicoReaderBase()
  , fEventCuts(new TStarJetPicoEventCuts)
  , fTrackCuts(new TStarJetPicoTrackCuts)
  , fTowerCuts(new TStarJetPicoTowerCuts)
  , fV0Cuts(new TStarJetPicoV0Cuts)
  , fRejectTowerElectrons(kTRUE)
  , fApplyMIPCorrection(kTRUE)
  , fApplyFractionHadronicCorrection(kFALSE)
  , fFractionHadronicCorrection(0.3)
{
  //
  // Default constructor
  //
}

TStarJetPicoReader::~TStarJetPicoReader()
{
  //
  // Destructor
  //

  delete fEventCuts;
  fEventCuts = 0;

  delete fV0Cuts;
  fV0Cuts = 0;

  delete fTrackCuts;
  fTrackCuts = 0;

  delete fTowerCuts;
  fTowerCuts = 0;
}


void TStarJetPicoReader::SetApplyMIPCorrection(Bool_t val) 
{
  //
  // Set flag to apply MIP correction fApplyMIPCorrection
  // - exclusive with fApplyFractionHadronicCorrection
  //

  fApplyMIPCorrection = val;
  if (fApplyMIPCorrection == kTRUE)
    {
      SetApplyFractionHadronicCorrection(kFALSE);
      __INFO("Enabling MIPCorrection");
    }
  else
    {
      __INFO("Disabling MIPCorrection");
    }
}

void TStarJetPicoReader::SetApplyFractionHadronicCorrection(Bool_t val) 
{
  //
  // Set flag to apply EMC hadronic correction fApplyFractionHadronicCorrection
  // - exclusive with fApplyMIPCorrection
  //

  fApplyFractionHadronicCorrection = val;
  if (fApplyFractionHadronicCorrection == kTRUE)
    {
      SetApplyMIPCorrection(kFALSE);
      __INFO("Enabling FractionHadronicCorrection");	    
    }
  else
    {
      __INFO("Disabling FractionHadronicCorrection");	    
    }
}

void TStarJetPicoReader::SetFractionHadronicCorrection(Double_t val) 
{
  //
  // Set value to fFractionHadronicCorrection (default is 0.3)
  // apply EMC hadronic correction fApplyFractionHadronicCorrection
  // - exclusive with fApplyMIPCorrection
  //
  
  fFractionHadronicCorrection = val;
  if (fFractionHadronicCorrection > 0.0 && fFractionHadronicCorrection <= 1.0)
    {
      SetApplyFractionHadronicCorrection(kTRUE);
      __INFO(Form("FractionHadronicCorrection %1.3f", fFractionHadronicCorrection));	    
    }
  else
    {
      SetApplyFractionHadronicCorrection(kFALSE);
    }
}

Bool_t TStarJetPicoReader::LoadEvent()
{
  if (fEventCuts->IsEventOK(fEvent) == kFALSE)
    {
      return kFALSE;
    }
  
  return kTRUE;
}

Bool_t TStarJetPicoReader::LoadV0s(TArrayI *trackIdsToRemove)
{
  //
  // Check QA and identify V0s
  // Add to surviving TStarJetVector to the output container
  // And assign indexes (trackIdsToRemove) for tracks not to double count
  //

  trackIdsToRemove->Reset(-1);
  Int_t trackIdsCounter = 0;

  TStarJetVector part;
  
  Int_t NacceptedV0s = 0;

  for (Int_t nv0 = 0; 
       nv0 < fEvent->GetHeader()->GetNOfV0s(); 
       nv0++)
    {
      TStarJetPicoV0 *pv0 = fEvent->GetV0(nv0);
      Double_t pt = TMath::Sqrt(pv0->GetPx()*pv0->GetPx() + pv0->GetPy()*pv0->GetPy());
      part.SetPtEtaPhiM(pt, pv0->GetEta(), pv0->GetPhi(), 0);
      part.SetType(TStarJetVector::_V0);
      part.SetCharge(TStarJetVector::_NEUTRAL);

      //
      // This will clone particles artificialy when v0 fulfils multiple cuts !!!
      // and flags are set to accept!
      //

      Int_t NacceptedV0sPerInput = 0;
      Bool_t isLambda = kFALSE;
      Bool_t isAntiLambda = kFALSE;
      Bool_t isK0 = kFALSE;
      
      if (fV0Cuts->AcceptLambdas() == kTRUE)
	{
	  if (fV0Cuts->DoLambdaPID(pv0) == __STARJETPICO_LAMBDA_PID)
	    {
	      part.SetPID(__STARJETPICO_LAMBDA_PID);
	      part.SetPicoMass(__STARJETPICO_MASS_LAMBDA);
	      fOutputContainer->Add(&part);
	      fSelectedV0s->AddLast(pv0);
	      NacceptedV0sPerInput++;
	      isLambda = kTRUE;
	      
	      trackIdsToRemove->AddAt(pv0->GetKeyPos(), trackIdsCounter++);
	      trackIdsToRemove->AddAt(pv0->GetKeyNeg(), trackIdsCounter++);	      	      
	    }
	} //lambdas
      
      if (fV0Cuts->AcceptAntiLambdas() == kTRUE)
	{
	  if (fV0Cuts->DoAntiLambdaPID(pv0) == __STARJETPICO_ANTILAMBDA_PID)
	    {
	      part.SetPID(__STARJETPICO_ANTILAMBDA_PID);
	      part.SetPicoMass(__STARJETPICO_MASS_LAMBDA);

	      fOutputContainer->Add(&part);
	      fSelectedV0s->AddLast(pv0);
	      NacceptedV0sPerInput++;
	      isAntiLambda = kTRUE;

	      trackIdsToRemove->AddAt(pv0->GetKeyPos(), trackIdsCounter++);
	      trackIdsToRemove->AddAt(pv0->GetKeyNeg(), trackIdsCounter++);	      
	    }
	} //anti-lambdas
      
      if (fV0Cuts->AcceptK0s() == kTRUE)
	{
	  if (fV0Cuts->DoK0sPID(pv0) == __STARJETPICO_K0_PID)
	    {
	      part.SetPID(__STARJETPICO_K0_PID);
	      part.SetPicoMass(__STARJETPICO_MASS_K0);

	      fOutputContainer->Add(&part);
	      fSelectedV0s->AddLast(pv0);
	      NacceptedV0sPerInput++;
	      isK0 = kTRUE;

	      trackIdsToRemove->AddAt(pv0->GetKeyPos(), trackIdsCounter++);
	      trackIdsToRemove->AddAt(pv0->GetKeyNeg(), trackIdsCounter++);	      
	    }
	} //k0s      

      if (NacceptedV0sPerInput > 1 && TStarJetPicoDefinitions::DebugLevel() > 10)
	{
	  __WARNING(Form("V0 accepted more than once! %2d", NacceptedV0sPerInput));
	  __WARNING(Form(" -> Is Lambda:%d Is AntiLambda:%d Is K0:%d",
			 isLambda, isAntiLambda, isK0));
	}

      NacceptedV0s += NacceptedV0sPerInput;
    }

  __DEBUG(9, Form("Accepted V0s: %d", NacceptedV0s));
  
  return kTRUE;
}

Bool_t TStarJetPicoReader::LoadTracks(TArrayI *trackIdsToRemove)
{
  //
  // Check track QA.
  // Do PID and set some features.
  // Add to surviving TStarJetVector to the output container.
  //

  TStarJetVector part;
  for (Int_t ntrack = 0; 
       ntrack < fEvent->GetHeader()->GetNOfPrimaryTracks(); 
       ntrack++)
    {      
      TStarJetPicoPrimaryTrack *ptrack = fEvent->GetPrimaryTrack(ntrack);

      // check if track taken by V0 - if yes skip it here
      if (IsKeyInArray(ptrack->GetKey(), trackIdsToRemove) == kTRUE)
	continue;

      if (fTrackCuts->IsTrackOK(ptrack) == kTRUE)
	{
	  Double_t pt = TMath::Sqrt(ptrack->GetPx()*ptrack->GetPx() + ptrack->GetPy()*ptrack->GetPy());
	  part.SetPtEtaPhiM(pt, ptrack->GetEta(), ptrack->GetPhi(), 0);
	  part.SetType(TStarJetVector::_TRACK);
	  part.SetCharge(ptrack->GetCharge());
	  
	  part.SetPID(fTrackCuts->DoPID(ptrack)); // not known yet...
	  part.SetFeatureD(TStarJetVector::_NSIGMA_PION, ptrack->GetNsigmaPion());
	  part.SetFeatureD(TStarJetVector::_NSIGMA_KAON, ptrack->GetNsigmaKaon());
	  part.SetFeatureD(TStarJetVector::_NSIGMA_PROTON, ptrack->GetNsigmaProton());      
	  part.SetFeatureD(TStarJetVector::_NSIGMA_ELECTRON, ptrack->GetNsigmaElectron()); 
	  part.SetFeatureD(TStarJetVector::_DEDX, ptrack->GetdEdx());

	  part.SetFeatureI(TStarJetVector::_KEY, ptrack->GetKey());
	  fOutputContainer->Add(&part);
	  fSelectedTracks->AddLast(ptrack);
	}
    }

  return kTRUE;
}

Bool_t TStarJetPicoReader::LoadTowers()
{
  //
  // Check tower QA.
  // Add electron PID to the tracks which are electrons - usage of FindVectorByKey()
  // Correct for hadronic showers.
  // Add to surviving TStarJetVector to the output container.
  // 

  TStarJetVector part;

  for (Int_t ntower = 0; 
       ntower < fEvent->GetHeader()->GetNOfTowers(); 
       ntower++)
    { 
      TStarJetPicoTower *ptower = fEvent->GetTower(ntower);
      Bool_t isElectronCandidate = kFALSE;
      if (fTowerCuts->IsTowerOK(ptower, fEvent) == kTRUE)
	{
	  // check the associated tracks
	  for (Int_t ntrack = 0; ntrack < ptower->GetNAssocTracks(); ntrack++)
	    {
	      Int_t idx = ptower->GetMatchedTrackIndex(ntrack);
	      TStarJetPicoPrimaryTrack *ptrack = fEvent->GetPrimaryTrack(idx);
	      if (fTrackCuts->IsTrackOK(ptrack) == kTRUE)
		{
		  Int_t key = ptrack->GetKey();
		  TStarJetVector *matchedTrack = TStarJetPicoReader::FindVectorByKey(key);
		      if (matchedTrack == 0)
			{
			  __ERROR(Form("For some reason this track was not accepted! Key: %d", key));
			  continue;
			}
		      matchedTrack->SetType(TStarJetVector::_MATCHED);
		      matchedTrack->SetPoverE(TStarJetPicoUtils::GetTowerPoverE(ptower, ptrack));
		      if (TStarJetPicoUtils::IsElectron(ptower, ptrack) == kTRUE)
			{
			  
			  matchedTrack->SetPID(__STARJETPICO_ELECTRON_PID);
			  isElectronCandidate = kTRUE; // tower is probably an electron
			} // is electron
		} // track is OK
	    } // loop over associated tracks

	  // do not accept electrons if necessary
	  if (fRejectTowerElectrons == kTRUE && isElectronCandidate == kTRUE)
	    continue;

	  // correct OR not the MIP
	  Double_t correctedEnergy = ptower->GetEnergy();
	  if (fApplyMIPCorrection == kTRUE)
	    {
	      correctedEnergy = fTowerCuts->TowerEnergyMipCorr(ptower);
	    }
	  else if (fApplyFractionHadronicCorrection == kTRUE)
	    {
	      correctedEnergy = fTowerCuts->HadronicCorrection(ptower,
							       fEvent,
							       fTrackCuts,
							       fFractionHadronicCorrection);	      
	    }
	  
	  // fill the container
	  if (correctedEnergy > 0)
	    {
	      Double_t mEt = correctedEnergy / TMath::CosH(ptower->GetEtaCorrected());
	      part.SetPtEtaPhiM(mEt, ptower->GetEtaCorrected(), ptower->GetPhiCorrected(), 0);
	      part.SetType(TStarJetVector::_TOWER);
	      part.SetCharge(TStarJetVector::_NEUTRAL);	      
	      part.SetPID(fTowerCuts->DoPID(ptower)); // not known yet...	      

	      fOutputContainer->Add(&part);
	      fSelectedTowers->AddLast(ptower);
	    }
	} // tower QA
    }

  return kTRUE;
}

void TStarJetPicoReader::SetEventCuts(TStarJetPicoEventCuts* parg)
{
  //
  // Set Event cuts. Should be an inheritted class from TStarJetPicoEventCuts.
  //

  // this is just a precaution... should not happen! ;)
  if (parg->IsA()->InheritsFrom("TStarJetPicoEventCuts") == kFALSE)
    {
      __ERROR("The pointer argument class should inherit from TStarJetPicoEventCuts");
      return;
    }

  if (fEventCuts != 0)
    {
      delete fEventCuts;
      fEventCuts = 0;
    }

  TString newName = parg->GetName();
  newName += "Cloned";
  TObject *tmp = parg->Clone(newName.Data());
  fEventCuts = dynamic_cast<TStarJetPicoEventCuts*>(tmp);
  if (fEventCuts != 0)
    {
      __INFO(Form("Event cuts replaced with an object of %s", parg->IsA()->GetName()));
    }
  else
    {
      __ERROR(Form("Unable to replace Event cuts with an object of %s", parg->IsA()->GetName()));
      __INFO("Reverting to default Event cuts");
      fEventCuts = new TStarJetPicoEventCuts;

      // cleanup
      delete tmp;
      tmp = 0;
    }
}

void TStarJetPicoReader::SetTrackCuts(TStarJetPicoTrackCuts* parg)
{
  //
  // Set Track cuts. Should be an inheritted class from TStarJetPicoTrackCuts.
  //

  // this is just a precaution... should not happen! ;)
  if (parg->IsA()->InheritsFrom("TStarJetPicoTrackCuts") == kFALSE)
    {
      __ERROR("The pointer argument class should inherit from TStarJetPicoTrackCuts");
      return;
    }

  if (fTrackCuts != 0)
    {
      delete fTrackCuts;
      fTrackCuts = 0;
    }

  TString newName = parg->GetName();
  newName += "Cloned";
  TObject *tmp = parg->Clone(newName.Data());
  fTrackCuts = dynamic_cast<TStarJetPicoTrackCuts*>(tmp);
  if (fTrackCuts != 0)
    {
      __INFO(Form("Track cuts replaced with an object of %s", parg->IsA()->GetName()));
    }
  else
    {
      __ERROR(Form("Unable to replace Track cuts with an object of %s", parg->IsA()->GetName()));
      __INFO("Reverting to default Track cuts");
      fTrackCuts = new TStarJetPicoTrackCuts;

      // cleanup
      delete tmp;
      tmp = 0;
    }
}

void TStarJetPicoReader::SetTowerCuts(TStarJetPicoTowerCuts* parg)
{
  //
  // Set Tower cuts. Should be an inheritted class from TStarJetPicoTowerCuts.
  //

  // this is just a precaution... should not happen! ;)
  if (parg->IsA()->InheritsFrom("TStarJetPicoTowerCuts") == kFALSE)
    {
      __ERROR("The pointer argument class should inherit from TStarJetPicoTowerCuts");
      return;
    }

  if (fTowerCuts != 0)
    {
      delete fTowerCuts;
      fTowerCuts = 0;
    }

  TString newName = parg->GetName();
  newName += "Cloned";
  TObject *tmp = parg->Clone(newName.Data());
  fTowerCuts = dynamic_cast<TStarJetPicoTowerCuts*>(tmp);
  if (fTowerCuts != 0)
    {
      __INFO(Form("Tower cuts replaced with an object of %s", parg->IsA()->GetName()));
    }
  else
    {
      __ERROR(Form("Unable to replace Tower cuts with an object of %s", parg->IsA()->GetName()));
      __INFO("Reverting to default Tower cuts");
      fTowerCuts = new TStarJetPicoTowerCuts;

      // cleanup
      delete tmp;
      tmp = 0;
    }
}

void TStarJetPicoReader::SetV0Cuts(TStarJetPicoV0Cuts* parg)
{
  //
  // Set V0 cuts. Should be an inheritted class from TStarJetPicoV0Cuts.
  //

  // this is just a precaution... should not happen! ;)
  if (parg->IsA()->InheritsFrom("TStarJetPicoV0Cuts") == kFALSE)
    {
      __ERROR("The pointer argument class should inherit from TStarJetPicoV0Cuts");
      return;
    }

  if (fV0Cuts != 0)
    {
      delete fV0Cuts;
      fV0Cuts = 0;
    }

  TString newName = parg->GetName();
  newName += "Cloned";
  TObject *tmp = parg->Clone(newName.Data());
  fV0Cuts = dynamic_cast<TStarJetPicoV0Cuts*>(tmp);
  if (fV0Cuts != 0)
    {
      __INFO(Form("V0 cuts replaced with an object of %s", parg->IsA()->GetName()));
    }
  else
    {
      __ERROR(Form("Unable to replace V0 cuts with an object of %s", parg->IsA()->GetName()));
      __INFO("Reverting to default V0 cuts");
      fV0Cuts = new TStarJetPicoV0Cuts;

      // cleanup
      delete tmp;
      tmp = 0;
    }
}
