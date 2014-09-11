/*
 * MFileReadOuts.cxx
 *
 *
 * Copyright (C) by Andreas Zoglauer.
 * All rights reserved.
 *
 *
 * This code implementation is the intellectual property of
 * Andreas Zoglauer.
 *
 * By copying, distributing or modifying the Program (or any work
 * based on the Program) you indicate your acceptance of this statement,
 * and all its terms.
 *
 */


////////////////////////////////////////////////////////////////////////////////
//
// MFileReadOuts
//
////////////////////////////////////////////////////////////////////////////////


// Include the header:
#include "MFileReadOuts.h"

// Standard libs:

// ROOT libs:

// MEGAlib libs:
#include "MGlobal.h"
#include "MStreams.h"
#include "MTokenizer.h"
#include "MFretalonRegistry.h"
#include "MReadOut.h"
#include "MReadOutElement.h"
#include "MReadOutElementDoubleStrip.h"
#include "MReadOutData.h"
#include "MReadOutDataADCValue.h"
#include "MReadOutDataADCValueWithTiming.h"


////////////////////////////////////////////////////////////////////////////////


#ifdef ___CINT___
ClassImp(MFileReadOuts)
#endif


////////////////////////////////////////////////////////////////////////////////


//! Default constructor
MFileReadOuts::MFileReadOuts() : MFile()
{
  // Construct an instance of MFileReadOuts
  
  m_FileType = "Unknown";
  m_Detector = "Unknown";
  m_Version = -1;
  m_StartObservationTime = MTime(0);
  m_EndObservationTime = MTime(0);
  m_StartClock = numeric_limits<long>::max();
  m_EndClock = numeric_limits<long>::max();
  
  m_NEventsInFile = 0;
  m_NGoodEventsInFile = 0;
  
  m_ROE = 0;
  m_ROD = 0;
}


////////////////////////////////////////////////////////////////////////////////


//! Default destructor
MFileReadOuts::~MFileReadOuts()
{
  delete m_ROE;
  delete m_ROD;

}


////////////////////////////////////////////////////////////////////////////////


//! Open the file
bool MFileReadOuts::Open(MString FileName, unsigned int Way)
{
  m_FileType = "ROA";
  if (MFile::Open(FileName, c_Read) == false) {
    return false;
  }
    
  bool Error = false;
  bool FoundVersion = false;
  bool FoundType = false;
  bool FoundUF = false;
  bool FoundTB = false;
  bool FoundCB = false;
  bool FoundTE = false;
  bool FoundCE = false;
  
  MString ReadOutElementFormat = "";
  MString ReadOutDataFormat = "";
  
  delete m_ROE;
  m_ROE = 0;
  delete m_ROD;
  m_ROD = 0;
  
  int Lines = 0;
  int MaxLines = 100;
  
  
  // Stage one: Find out what kind of file we have
  
  MString Line;
  while (m_File.good() == true) {
    
    if (++Lines >= MaxLines) break;
    Line.ReadLine(m_File);
    
    if (FoundType == false) {
      if (Line.BeginsWith("TY") == true) {
        MTokenizer Tokens;
        Tokens.Analyze(Line);
        if (Tokens.GetNTokens() != 2) {
          mout<<"Error while opening file "<<m_FileName<<": "<<endl;
          mout<<"Unable to read file type (should be "<<m_FileType<<")"<<endl;
          Error = true;
        } else {
          m_FileType = Tokens.GetTokenAtAsString(1);
          m_FileType.ToLower();
          FoundType = true;
          //mout<<"Found calibration file Type: "<<m_FileType<<endl;
        }
      }
    }
    if (FoundVersion == false) {
      if (Line.BeginsWith("VE") == true) {
        MTokenizer Tokens;
        Tokens.Analyze(Line);
        if (Tokens.GetNTokens() != 3) {
          mout<<"Error while opening file "<<m_FileName<<": "<<endl;
          mout<<"Unable to read file version."<<endl;              
          Error = true;
        } else {
          m_Detector = Tokens.GetTokenAtAsString(1);
          m_Detector.ToLower();
          m_Version = Tokens.GetTokenAtAsInt(2);
          FoundVersion = true;
          //mout<<"Found dat file version: "<<m_Version<<" for detector "<<m_Detector<<endl;
        }
      }
    }
    if (FoundTB == false) {
      if (Line.BeginsWith("TB") == true) {
        MTokenizer Tokens;
        Tokens.Analyze(Line);
        if (Tokens.GetNTokens() != 2) {
          mout<<"Error while opening file "<<m_FileName<<": "<<endl;
          mout<<"Unable to read TB keyword"<<endl;              
          Error = true;
        } else {
          m_StartObservationTime = Tokens.GetTokenAtAsDouble(1);
          FoundTB = true;
        }
      }
    }
    if (FoundUF == false) {
      if (Line.BeginsWith("UF") == true) {
        MTokenizer Tokens;
        Tokens.Analyze(Line);
        if (Tokens.GetNTokens() != 3) {
          mout<<"Error while opening file "<<m_FileName<<": "<<endl;
          mout<<"Unable to read UF keyword"<<endl;              
          Error = true;
        } else {
          ReadOutElementFormat = Tokens.GetTokenAt(1);
          ReadOutDataFormat = Tokens.GetTokenAt(2);
          FoundUF = true;
          //cout<<"Found: read-out element format: "<<ReadOutElementFormat<<", read-out data format: "<<ReadOutDataFormat<<endl;
        }
      }
    }
    if (FoundCB == false) {
      if (Line.BeginsWith("CB") == true) {
        MTokenizer Tokens;
        Tokens.Analyze(Line);
        if (Tokens.GetNTokens() != 2) {
          mout<<"Error while opening file "<<m_FileName<<": "<<endl;
          mout<<"Unable to read file version."<<endl;              
          Error = true;
        } else {
          m_StartClock = Tokens.GetTokenAtAsDouble(1);
          FoundCB = true;
        }
      }
    }
  }
  MFile::Rewind();
  
  // Take care of redundant formats:
  if (m_Detector == "GRIPS2013") {
    ReadOutElementFormat = "doublesidedstrip";
    ReadOutDataFormat = "datawithtiming";
  }
  
  // Create the read-out elements and data to fill
  if ((m_ROE = MFretalonRegistry::Instance().GetReadOutElement(ReadOutElementFormat)) == 0) {
    cout<<"No read-out element of type "<<ReadOutElementFormat<<" is registered!"<<endl;
    return false;
  }
  if ((m_ROD = MFretalonRegistry::Instance().GetReadOutData(ReadOutDataFormat)) == 0) {
    cout<<"No read-out data of type "<<ReadOutDataFormat<<" is registered!"<<endl;
    return false;
  }
  
  
  
  // Now go to the end of the file to find the TE, CE keywords
  m_File.clear();
  if (m_FileLength > (streampos) 10000) {
    m_File.seekg(m_FileLength - streamoff(10000));
  } else {
    // start from the beginning...
    MFile::Rewind();
  }
  Line.ReadLine(m_File); // Ignore the first line
  while (m_File.good() == true) {
    Line.ReadLine(m_File);
    if (Line.Length() < 2) continue;
    
    if (FoundTE == false) {
      if (Line[0] == 'T' && Line[1] == 'E') {
        MTokenizer Tokens;
        Tokens.Analyze(Line);
        if (Tokens.GetNTokens() != 2) {
          mout<<"Error while opening file "<<m_FileName<<": "<<endl;
          mout<<"Unable to read TE keyword"<<endl;              
          Error = true;
        } else {
          m_EndObservationTime = Tokens.GetTokenAtAsDouble(1);
          FoundTE = true;
        }
      }
    }
    if (FoundCE == false) {
      if (Line[0] == 'C' && Line[1] == 'E') {
        MTokenizer Tokens;
        Tokens.Analyze(Line);
        if (Tokens.GetNTokens() != 2) {
          mout<<"Error while opening file "<<m_FileName<<": "<<endl;
          mout<<"Unable to read CE keyword"<<endl;              
          Error = true;
        } else {
          m_EndClock = Tokens.GetTokenAtAsDouble(1);
          FoundCE = true;
        }
      }
    }
  }
  MFile::Rewind();
  
  // Now do the sanity checks:
  if (m_FileType != "dat" && m_FileType != "roa") {
    mout<<"Error while opening file "<<m_FileName<<": "<<endl;
    mout<<"The file type must be \"dat\" or \"roa\" (case is ignored) - you have \""<<m_FileType<<"\""<<endl;              
    Error = true;
    return false;
  }
  /*
  if (FoundTB == false) {
    mout<<"Error while opening file "<<m_FileName<<": "<<endl;
    mout<<"Did not find the start time in the file (TB keyword)"<<endl;              
    Error = true;
  }
  if (FoundTE == false) {
    mout<<"Error while opening file "<<m_FileName<<": "<<endl;
    mout<<"Did not find the end time in the file (TE keyword)"<<endl;              
    Error = true;
  }
  if (FoundCB == false) {
    mout<<"Error while opening file "<<m_FileName<<": "<<endl;
    mout<<"Did not find the start clock in the file (CB keyword)"<<endl;              
    Error = true;
  }
  if (FoundCE == false) {
    mout<<"Error while opening file "<<m_FileName<<": "<<endl;
    mout<<"Did not find the end clock in the file (CE keyword)"<<endl;              
    Error = true;
  }
  if (m_StartObservationTime > m_EndObservationTime) {
    mout<<"Error while opening file "<<m_FileName<<": "<<endl;
    mout<<"The start of the observation time is larger than its end!"<<endl;              
    Error = true;
  }
  */
  
  return !Error;
}


////////////////////////////////////////////////////////////////////////////////


bool MFileReadOuts::ReadNext(MReadOutSequence& ROS, int SelectedDetectorID)
{
  // Return next single event from file... or 0 if there are no more.

  ROS.Clear();
  
  bool Error = false;
  MString Line;
  
  if (UpdateProgress(50) == false) {
    return 0;
  }
  
  // Read file line-by-line, returning 'Event' when it's read a complete, non-empty event.
  while (m_File.good() == true) {
    Line.ReadLine(m_File);
    if (Line.Length() < 2) continue;
    //mout<<Line<<endl;
          
    // Case 1: The event is completed.  Check to see if we're at the following "SE".
    if (Line[0] == 'S' && Line[1] == 'E') {
      // If the event is empty, then we ignore it and prepare for the next event:
      //mout << "MNCTFileEventsDat::ReadNextEvent: Done reading event" << endl;
      m_NEventsInFile++;
      if (ROS.GetNumberOfReadOuts() == 0) {
        ROS.Clear();
      } else {
        // Done reading a non-empty event.  Return it:
        //mout<<"MNCTFileEventsDat::ReadNextEvent: Returning good event: "<<long(Event)<<endl;
        m_NGoodEventsInFile++;
        if (Error == true) {
          mout<<"An error occured during reading the event with ID "<<ROS.GetID()<<endl;
          mout<<"(If the error is really bad, then there might event not be an ID)"<<endl;
          mout<<"I pass the event on anyway."<<endl;
        }
        return !Error;
      }
    } // SE
      
    // Case 2: Parse other keywords in the event
    if (Line[0] == 'I' && Line[1] == 'D') {
      unsigned long ID = 0;
      if (sscanf(Line.Data(), "ID %lu\n", &ID) == 1) {
        ROS.SetID(ID);
      } else {
        Error = true;
      }
    } else if (Line[0] == 'T' && Line[1] == 'I') {
      MTime T(0);
      if (T.Set(Line) == true) {
        ROS.SetTime(T);
      } else {
        Error = true;
      }
    } else if (Line[0] == 'C' && Line[1] == 'L') {
      unsigned long Clock = 0;
      if (sscanf(Line.Data(), "CL %lu\n", &Clock) == 1) {
        ROS.SetClock(Clock);
      } else {
        Error = true;
      }
    } else if (Line[0] == 'U' && Line[1] == 'H') {
      MTokenizer T(' ', false);
      T.Analyze(Line, false);
        
      m_ROE->Parse(T, 1);
      m_ROD->Parse(T, 1 + m_ROE->GetNumberOfParsableElements());
      
      if (SelectedDetectorID < 0 || (SelectedDetectorID >= 0 && (int) m_ROE->GetDetectorID() == SelectedDetectorID)) {
        MReadOut RO(*m_ROE, *m_ROD);
        ROS.AddReadOut(RO);
      }
        
    } else if (Line[0] == 'S' && Line[1] == 'H') {
      int DetectorID = 0;
      char PosOrNeg = 'a';
      int StripID = 0;
      int Triggered = 0;
      unsigned long Timing = 0;
      long UncorrectedADC = 0;
      long CorrectedADC = 0;
      if (sscanf(Line.Data(), "SH %d %c %d %d %lu %li %li\n", &DetectorID, &PosOrNeg, &StripID, &Triggered, &Timing, &UncorrectedADC, &CorrectedADC) == 7) {
        if (SelectedDetectorID < 0 || (SelectedDetectorID >= 0 && DetectorID == SelectedDetectorID)) {
          const MReadOutElement& ROE = MReadOutElementDoubleStrip(DetectorID, StripID, PosOrNeg == 'p');
          const MReadOutData& ROD = MReadOutDataADCValueWithTiming((double) CorrectedADC, (double) Timing);

          MReadOut RO(ROE, ROD);
          ROS.AddReadOut(RO);
        }
        //if (StripID == 13) {
        //  cout<<RO<<endl;
        //}
      } else {
        Error = true;
      }
    }
    
  } // End of while(m_File.good() == true)
  
  // Done reading.  No more new events.
  if (ROS.GetNumberOfReadOuts() == 0) {
    ROS.Clear();
  } else {
    // Done reading a non-empty event.  Return it:
    //mout << "MNCTFileEventsDat::GetNextEvent: Returning good event (at end of function)" << endl;
    m_NGoodEventsInFile++;
    if (Error == true) {
      mout<<"An error occured during reading the event with ID "<<ROS.GetID()<<endl;
      mout<<"(If the error is really bad, then there might event not be an ID)"<<endl;
      mout<<"I pass the event on anyway."<<endl;
    }
    return !Error;
  }
  
  //cout<<"Returning 0"<<endl;
  
  return false;
}


// MFileReadOuts.cxx: the end...
////////////////////////////////////////////////////////////////////////////////