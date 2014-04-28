
#include <TSystem.h>
#include <TF1.h>
#include <TGraph.h>
#include <TGraphErrors.h>
#include <TH2D.h>
#include <TProfile.h>
#include <TCanvas.h>
#include <TPavesText.h>
#include <TLegend.h>
#include <TLatex.h>
#include <Riostream.h>

// #include <cstdlib>

#include "libxml/parser.h"
#include "libxml/xmlmemory.h"
// #include "libxml/xmlreader.h"

#include "Utils/SystemUtils.h"
#include "Messenger/Messenger.h"
#include "PDG/PDGCodes.h"

#include "ExpData.h"

using namespace genie;
using namespace genie::mc_vs_data;

ExpData::ExpData()
{

   // this are "global" things
   //
   std::string GDir = gSystem->Getenv("GENIE"); // (it's a const char* return type ) 
   fExpDataDirPath = GDir + "/data/validation";
   
   
   // should this be configurable ???
   //
   fExpDataDirPath += "/vA"; // this is nu+A, and... in principle, should this be determined by the input/config ???
   fExpDataDirPath += "/hadronics";
   
   fCurrentIntType = kInvalid;
      
}

bool ExpData::LoadExpData( const std::string& dset )
{
   
   // NOTE: In principle, I can use xmlTextReaderPtr but that's
   //       simply a typedef to a bare xmlTextReader pointer !!!
   //
   xmlTextReader* reader = 0;
   reader = xmlNewTextReaderFilename(dset.c_str());
   if(reader == NULL) 
   {
      xmlFree(reader);
      return false;
   }

   int ret = 0;
   
   while ( reader != NULL )
   {

      ret = xmlTextReaderRead(reader);
      while (ret == 1) 
      {

         xmlChar*   name  = xmlTextReaderName     (reader);
         int        type  = xmlTextReaderNodeType (reader);
	 
	 // end of exp.data input
	 //
	 if ( xmlStrEqual(name,(const xmlChar*)"exp_data") && type==XML_READER_TYPE_END_ELEMENT )
	 {
	    xmlFree(name);
	    xmlFreeTextReader(reader);
	    return true;
	 }
	 
         while ( (!(xmlStrEqual(name,(const xmlChar*)"interaction") && type==XML_READER_TYPE_ELEMENT )) && ret==1 )
	 {	 
	    ret = xmlTextReaderRead(reader);
            name  = xmlTextReaderName     (reader);
            type  = xmlTextReaderNodeType (reader);
	 } 
	 	 
	 // found another "interaction" record
	 // reset interaction type
	 //
	 fCurrentIntType = kInvalid;
	 
	 bool code = ProcessRecord(reader);
         if ( !code || ret!=1 )
         {
            xmlFree(name);
	    xmlFreeTextReader(reader);
	    if ( code )
	    {
	       return true; // normal case, just EOF
	    }
	    else
	    {
	       return false;
	    }
         }
	 
	 ret = xmlTextReaderRead(reader);

	 xmlFree(name);

      } // end loop over ret=1
      
      xmlFreeTextReader(reader);

   } // end loop on reader!=0
      
   return true;

}

// NOTE: just in case...
//
//Enum xmlReaderTypes {
//    XML_READER_TYPE_NONE = 0
//    XML_READER_TYPE_ELEMENT = 1
//    XML_READER_TYPE_ATTRIBUTE = 2
//    XML_READER_TYPE_TEXT = 3
//    XML_READER_TYPE_CDATA = 4
//    XML_READER_TYPE_ENTITY_REFERENCE = 5
//    XML_READER_TYPE_ENTITY = 6
//    XML_READER_TYPE_PROCESSING_INSTRUCTION = 7
//    XML_READER_TYPE_COMMENT = 8
//    XML_READER_TYPE_DOCUMENT = 9
//    XML_READER_TYPE_DOCUMENT_TYPE = 10
//    XML_READER_TYPE_DOCUMENT_FRAGMENT = 11
//    XML_READER_TYPE_NOTATION = 12
//    XML_READER_TYPE_WHITESPACE = 13
//    XML_READER_TYPE_SIGNIFICANT_WHITESPACE = 14
//    XML_READER_TYPE_END_ELEMENT = 15
//    XML_READER_TYPE_END_ENTITY = 16
//    XML_READER_TYPE_XML_DECLARATION = 17
//}

bool ExpData::ProcessRecord( xmlTextReader* reader )
{

// NOTE: Do NOT free the xml reader here !!!

   xmlChar*   name  = xmlTextReaderName     (reader);
   int        type  = xmlTextReaderNodeType (reader);
//   int        depth = xmlTextReaderDepth    (reader);   
       
   // re-init of the interaction record
   //
   if ( (!xmlStrcmp(name, (const xmlChar*)"interaction")) && type==XML_READER_TYPE_ELEMENT )
   {
      xmlChar* projectile = xmlTextReaderGetAttribute(reader,(const xmlChar*)"projectile");
      xmlChar* target     = xmlTextReaderGetAttribute(reader,(const xmlChar*)"target");
      CheckInteractionType( projectile, target );  
      xmlFree(projectile);
      xmlFree(target);   
   }
   
   int ret = xmlTextReaderRead(reader);
   
   // loop over the interaction record
   // break the loop if at teh end of interaction record
   //
   while ( ret == 1 ) // keep reading till the end of interaction
   {
      name = xmlTextReaderName     (reader);
      type = xmlTextReaderNodeType (reader);
      
      if ( xmlStrEqual(name,(const xmlChar*)"interaction") && type==XML_READER_TYPE_END_ELEMENT ) break;

      // init the incoming dataset
      //
      if ( xmlStrEqual(name, (const xmlChar*)"dataset") && type==XML_READER_TYPE_ELEMENT )
      {
         // reset current dataset location, reference & graph ptr
         //
         fCurrentDSLocation = "";
         fCurrentDSReference = "";
         fCurrentGraph = 0;      
         
	 // cycle over the contents of the incoming dataset
         //
         while ( !( xmlStrEqual(name,(const xmlChar*)"dataset") && type==XML_READER_TYPE_END_ELEMENT )  )
         {
            ret = xmlTextReaderRead(reader);
	    if ( ret != 1 )
	    {
	       xmlFree(name);
	       return false;
	    }
	    xmlChar* name1 = xmlTextReaderName(reader);
	    int      type1 = xmlTextReaderNodeType(reader);
	    if ( xmlStrEqual(name1,(const xmlChar*)"path") && type1==XML_READER_TYPE_ELEMENT )
	    {
	       std::string dir;
	       dir.assign( (const char*)xmlTextReaderGetAttribute(reader,(const xmlChar*)"dir") );
	       fCurrentDSLocation = fExpDataDirPath + dir;
	    }
	    if ( xmlStrEqual(name1, (const xmlChar*)"reference") && type1==XML_READER_TYPE_ELEMENT )
	    {
	       fCurrentDSReference.assign( (const char*)xmlTextReaderGetAttribute(reader,(const xmlChar*)"ref") );
	    }
	    if ( xmlStrEqual( name1, (const xmlChar*)"plot") && type1==XML_READER_TYPE_ELEMENT )
	    {
	       xmlChar* title  = 0;
	       xmlChar* xtitle = 0;
	       xmlChar* xlow   = 0;
	       xmlChar* xup    = 0;
	       xmlChar* ytitle = 0;
	       xmlChar* ylow   = 0;
	       xmlChar* yup    = 0;
	       //
	       // cycle over plots specs
	       //
	       while ( !( xmlStrEqual(name1,(const xmlChar*)"plot") && type1==XML_READER_TYPE_END_ELEMENT ) )
	       {
	          ret = xmlTextReaderRead(reader);
	          if ( ret != 1 )
	          {
	             xmlFree(name);
		     xmlFree(name1);
		     xmlFree(title);
		     xmlFree(xtitle);
		     xmlFree(xlow);
		     xmlFree(xup);
		     xmlFree(ytitle);
		     xmlFree(ylow);
		     xmlFree(yup);
		     return false;
	          }
	          xmlChar* name2 = xmlTextReaderName(reader);
	          if ( xmlStrEqual(name2,(const xmlChar*)"observable") )
	          { 
	             std::string variable;
		     variable.append( (const char*)xmlTextReaderGetAttribute(reader,(const xmlChar*)"name") );
		     AddExpData( fCurrentIntType, variable );		  
	          }
	          if ( xmlStrEqual(name2,(const xmlChar*)"title") )
	          {
	             title = xmlTextReaderGetAttribute(reader,(const xmlChar*)"name");
	          }
	          if ( xmlStrEqual(name2,(const xmlChar*)"xaxis") )
	          {
	             xtitle   = xmlTextReaderGetAttribute(reader,(const xmlChar*)"name");
	             xlow     = xmlTextReaderGetAttribute(reader,(const xmlChar*)"xlow");
	             xup      = xmlTextReaderGetAttribute(reader,(const xmlChar*)"xup");
	          }
	          if ( xmlStrEqual(name2,(const xmlChar*)"yaxis") )
	          {
	             ytitle   = xmlTextReaderGetAttribute(reader,(const xmlChar*)"name");
	             ylow     = xmlTextReaderGetAttribute(reader,(const xmlChar*)"ylow");
	             yup      = xmlTextReaderGetAttribute(reader,(const xmlChar*)"yup");
	          }
		  // update
		  name1 = xmlTextReaderName(reader);
		  type1 = xmlTextReaderNodeType(reader);
	       
	          xmlFree(name2);
	       
	       } // end cycle over plot specs
	       //
	       // finilize plot specs
	       // check if all the essencials have been set
	       // otherwise bail out !!!
	       //
	       if ( !( title && xtitle && xlow && xup && fCurrentGraph ) )
	       {
	          xmlFree(name);
		  xmlFree(name1);
		  xmlFree(title);
		  xmlFree(xtitle);
		  xmlFree(xlow);
		  xmlFree(xup);
		  xmlFree(ytitle);
		  xmlFree(ylow);
		  xmlFree(yup);
	          return false;
	       }
	       fCurrentGraph->SetTitle( (const char*)title );
	       fCurrentGraph->GetXaxis()->SetTitle( (const char*)xtitle );
	       float x1 = atof( (const char*)xlow );
	       float x2 = atof( (const char*)xup );
	       fCurrentGraph->GetXaxis()->SetLimits( x1, x2 );
	       float y1 = atof( (const char*)ylow );
	       float y2 = atof( (const char*)yup );
	       // FIXME: neither approach seems to work as the limits on the 
	       //        final plots (overlay) aren't as set here... 
	       //        need to double check details...
	       fCurrentGraph->GetYaxis()->SetLimits( y1, y2 );
//	       fCurrentGraph->GetYaxis()->SetRangeUser( y1, y2 );
	    
	       xmlFree(title);
	       xmlFree(xtitle);
	       xmlFree(xlow);
	       xmlFree(xup);	    
	       xmlFree(ytitle);
	       xmlFree(ylow);
	       xmlFree(yup);	    
	    } // end-if for the plot	    
	    //
	    // update name & type to the current position
	    //
	    name = xmlTextReaderName(reader);
	    type = xmlTextReaderNodeType(reader);
	    
	    xmlFree(name1);	    
         } // end cycle over given dataset
      
      } // end-if for the start of a new dataset
      
      // move to the next tag
      //
      ret = xmlTextReaderRead(reader);
   
   } // loop over valid read-in (ret==1)
   
   xmlFree(name);

// TEST:
//
/*
   std::map< InteractionType, std::map< string, std::vector<std::string> > >::const_iterator i = fExpDataHolderTest.find(fCurrentIntType);
   if ( i != fExpDataHolderTest.end() )
   {   
      std::map< std::string, std::vector<std::string> >::const_iterator itr = (i->second).find("ChHad_W2");
      int NDataSets = (itr->second).size();
      std::cout << " Number of datasets in STORAGE = " << NDataSets << std::endl; 
   }
   else
   {      
      LOG("gvldtest", pERROR) << " NO EXP.DATA FOUND IN TEST STORAGE FOR INTERACTION TYPE " << fCurrentIntType;
   }
*/
   return true;

}

void ExpData::AddExpData( const InteractionType& type, const std::string& var )
{

   std::map< InteractionType, std::map< std::string, std::vector<std::string> > >::iterator i1 = fExpDataHolderTest.find(type);   
   
   // check if data set(s) exist(s) for this interaction type
   // if not, just add a new element
   //
   if ( i1 == fExpDataHolderTest.end() )
   {
      fExpDataHolderTest[type][var].push_back( fCurrentDSLocation );
      fCurrentGraph = MakeGraph( type, var );
      return;
   }

   // data for this interaction type do exist
   //
   std::map< std::string, std::vector<std::string> >::iterator i2 = (i1->second).find(var);

   // check if data exist for this observable
   // if not, just make a new entry
   //
   if ( i2 == (i1->second).end() )
   {
      ((i1->second)[var]).push_back( fCurrentDSLocation );
      fCurrentGraph = MakeGraph( type, var );
      return;
   } 
   
   int NEntries = (i2->second).size();
   for ( int ie=0; ie<NEntries; ++ie )
   {
      if ( (i2->second)[ie] == fCurrentDSLocation ) 
      {
         // --> fCurrentGraph = ffExpDataHolderTest[type][var][ie]; // do I need it ???
	 return; // don't add this dataset as it's already in !!!
      }
   }
   
   (i2->second).push_back( fCurrentDSLocation );
   fCurrentGraph = MakeGraph( type, var );

   return;

}

const std::map< std::string, std::vector<std::string> >* ExpData::GetExpDataNames( const InteractionType& type ) const
{

   if ( type == kInvalid ) return NULL;
   
   std::map< InteractionType, std::map< std::string, std::vector<std::string> > >::const_iterator i = fExpDataHolderTest.find(type);

   if ( i != fExpDataHolderTest.end() )
   {
      return &(i->second);
   }

   return NULL;

}

const std::map< std::string, std::vector<TGraphErrors*> >* ExpData::GetExpDataGraphs( const InteractionType& type ) const
{

   if ( type == kInvalid ) return NULL;
   
   std::map< InteractionType, std::map< std::string, std::vector<TGraphErrors*> > >::const_iterator i = fGraphsTest.find(type);
   
   if ( i != fGraphsTest.end() )
   {
      return &(i->second);
   }
   
   return NULL;

}

ExpData::InteractionType ExpData::CheckInteractionType( const xmlChar* projectile, const xmlChar* target )
{
   
   if ( xmlStrEqual( projectile, (const xmlChar*)"nu" ) )
   {
      if      ( xmlStrEqual( target, (const xmlChar*)"proton" ) )  { fCurrentIntType = ExpData::kNuP; }
      else if ( xmlStrEqual( target, (const xmlChar*)"neutron" ) ) { fCurrentIntType = ExpData::kNuN; }
   }
   else if ( xmlStrEqual( projectile, (const xmlChar*)"nubar" ) )
   {
      if      ( xmlStrEqual( target, (const xmlChar*)"proton" ) )  { fCurrentIntType = ExpData::kNubarP; }
      else if ( xmlStrEqual( target, (const xmlChar*)"neutron" ) ) { fCurrentIntType = ExpData::kNubarN; }
   }
   
   return fCurrentIntType;
   
}

TGraphErrors* ExpData::MakeGraph( const InteractionType& type, const std::string& var ) // std::string& file )
{

  // fragment copied over from the original code, 
  // (with just a couple of small modifications)
  //
  // string filename = fExpDataDirPath + file;

  bool file_exists = !(gSystem->AccessPathName(fCurrentDSLocation.c_str()));
  if(!file_exists) {
    LOG("gvldtest", pERROR) << "Can not find file: " << fCurrentDSLocation;
    return NULL;
  }

  ifstream in;
  in.open(fCurrentDSLocation.c_str());
  double x, y, ex, ey;
  vector<double> vx;
  vector<double> vy;
  vector<double> vex;
  vector<double> vey;
  while (1) 
  {
    in >>x >> y >>ey;
    ex = 0;
    if (!in.good()) break;
    vx.push_back(x);
    vy.push_back(y);
    vex.push_back(ex);
    vey.push_back(ey);
  }
  
  in.close();

  fGraphsTest[type][var].push_back( new TGraphErrors(vx.size(),&vx[0],&vy[0],&vex[0],&vey[0]) ); 
    
  return fGraphsTest[type][var].back();
   
}