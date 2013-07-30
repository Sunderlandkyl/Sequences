/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// MetafileImporter Logic includes
#include "vtkSlicerMetafileImporterLogic.h"
#include "vtkSlicerMultiDimensionLogic.h"

// MRML includes

// VTK includes
#include <vtkNew.h>
#include "vtkMatrix4x4.h"
#include "vtkMRMLLinearTransformNode.h"
#include "vtkMRMLScalarVolumeNode.h"
#include "vtkImageData.h"
#include "vtkExtractVOI.h"
#include "vtkMRMLScalarVolumeDisplayNode.h"
#include "vtkSmartPointer.h"

// STD includes
#include <cassert>
#include <sstream>



//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkSlicerMetafileImporterLogic);

//----------------------------------------------------------------------------
vtkSlicerMetafileImporterLogic
::vtkSlicerMetafileImporterLogic() 
{
  this->MultiDimensionLogic = NULL;
}

//----------------------------------------------------------------------------
vtkSlicerMetafileImporterLogic::~vtkSlicerMetafileImporterLogic()
{
}

//----------------------------------------------------------------------------
void vtkSlicerMetafileImporterLogic::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//---------------------------------------------------------------------------
void vtkSlicerMetafileImporterLogic::SetMRMLSceneInternal(vtkMRMLScene * newScene)
{
  vtkNew<vtkIntArray> events;
  events->InsertNextValue(vtkMRMLScene::NodeAddedEvent);
  events->InsertNextValue(vtkMRMLScene::NodeRemovedEvent);
  events->InsertNextValue(vtkMRMLScene::EndBatchProcessEvent);
  this->SetAndObserveMRMLSceneEventsInternal(newScene, events.GetPointer());
}

//-----------------------------------------------------------------------------
void vtkSlicerMetafileImporterLogic::RegisterNodes()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerMetafileImporterLogic::UpdateFromMRMLScene()
{
  assert(this->GetMRMLScene() != 0);
}

//---------------------------------------------------------------------------
void vtkSlicerMetafileImporterLogic
::OnMRMLSceneNodeAdded(vtkMRMLNode* vtkNotUsed(node))
{
}

//---------------------------------------------------------------------------
void vtkSlicerMetafileImporterLogic
::OnMRMLSceneNodeRemoved(vtkMRMLNode* vtkNotUsed(node))
{
}






// Add the helper functions

//-------------------------------------------------------
void Trim(std::string &str)
{
  str.erase(str.find_last_not_of(" \t\r\n")+1);
  str.erase(0,str.find_first_not_of(" \t\r\n"));
}

//-------------------------------------------------------
std::string Trim(const char* c)
{
  std::string str = c;
  str.erase(str.find_last_not_of(" \t\r\n")+1);
  str.erase(0,str.find_first_not_of(" \t\r\n"));
  return str;
}

//----------------------------------------------------------------------------
/*! Quick and robust string to int conversion */
template<class T>
void StringToInt(const char* strPtr, T &result)
{
  if (strPtr==NULL || strlen(strPtr) == 0 )
  {
    return;
  }
  char * pEnd=NULL;
  result = static_cast<int>(strtol(strPtr, &pEnd, 10));
  if (pEnd != strPtr+strlen(strPtr) )
  {
    return;
  }
  return;
}


vtkMRMLLinearTransformNode* StringToTransformNode( std::string str )
{
  std::stringstream ss( str );
  
  double e00; ss >> e00; double e01; ss >> e01; double e02; ss >> e02; double e03; ss >> e03;
  double e10; ss >> e10; double e11; ss >> e11; double e12; ss >> e12; double e13; ss >> e13;
  double e20; ss >> e20; double e21; ss >> e21; double e22; ss >> e22; double e23; ss >> e23;
  double e30; ss >> e30; double e31; ss >> e31; double e32; ss >> e32; double e33; ss >> e33;

  vtkMatrix4x4* matrix = vtkMatrix4x4::New();

  matrix->SetElement( 0, 0, e00 );
  matrix->SetElement( 0, 1, e01 );
  matrix->SetElement( 0, 2, e02 );
  matrix->SetElement( 0, 3, e03 );

  matrix->SetElement( 1, 0, e10 );
  matrix->SetElement( 1, 1, e11 );
  matrix->SetElement( 1, 2, e12 );
  matrix->SetElement( 1, 3, e13 );

  matrix->SetElement( 2, 0, e20 );
  matrix->SetElement( 2, 1, e21 );
  matrix->SetElement( 2, 2, e22 );
  matrix->SetElement( 2, 3, e23 );

  matrix->SetElement( 3, 0, e30 );
  matrix->SetElement( 3, 1, e31 );
  matrix->SetElement( 3, 2, e32 );
  matrix->SetElement( 3, 3, e33 );

  vtkMRMLLinearTransformNode* transformNode = vtkMRMLLinearTransformNode::New();
  transformNode->SetAndObserveMatrixTransformToParent( matrix );

  return transformNode;
}


// Constants for reading transforms
static const int MAX_LINE_LENGTH = 1000;

static std::string SEQMETA_FIELD_FRAME_FIELD_PREFIX = "Seq_Frame";
static std::string SEQMETA_FIELD_IMG_STATUS = "ImageStatus";

//----------------------------------------------------------------------------
void vtkSlicerMetafileImporterLogic
::ReadTransforms( std::string fileName )
{

  // Open in binary mode because we determine the start of the image buffer also during this read
  const char* flags = "rb";
  FILE* stream = fopen( fileName.c_str(), flags ); // TODO: Removed error

  char line[ MAX_LINE_LENGTH + 1 ] = { 0 };

  while ( fgets( line, MAX_LINE_LENGTH, stream ) )
  {

    std::string lineStr = line;

    // Split line into name and value
    size_t equalSignFound;
    equalSignFound = lineStr.find_first_of( "=" );
    if ( equalSignFound == std::string::npos )
    {
      // vtkWarningMacro("Parsing line failed, equal sign is missing ("<<lineStr<<")");
      continue;
    }
    std::string name = lineStr.substr( 0, equalSignFound );
    std::string value = lineStr.substr( equalSignFound + 1 );

    // Trim spaces from the left and right
    Trim( name );
    Trim( value );

    // Only consider the Seq_Frame
    if ( name.compare( 0, SEQMETA_FIELD_FRAME_FIELD_PREFIX.size(), SEQMETA_FIELD_FRAME_FIELD_PREFIX ) != 0 )
    {
      continue;
    }

    // frame field
    // name: Seq_Frame0000_CustomTransform
    name.erase( 0, SEQMETA_FIELD_FRAME_FIELD_PREFIX.size() ); // 0000_CustomTransform

    // Split line into name and value
    size_t underscoreFound;
    underscoreFound = name.find_first_of( "_" );
    if ( underscoreFound == std::string::npos )
    {
      // vtkWarningMacro("Parsing line failed, underscore is missing from frame field name ("<<lineStr<<")");
      continue;
    }

    std::string frameNumberStr = name.substr( 0, underscoreFound ); // 0000
    std::string frameFieldName = name.substr( underscoreFound + 1 ); // CustomTransform

    int frameNumber = 0;
    StringToInt( frameNumberStr.c_str(), frameNumber ); // TODO: Removed warning
      
    // Convert the string to transform and add transform to hierarchy
    if ( frameFieldName.find( "Transform" ) != std::string::npos && frameFieldName.find( "Status" ) == std::string::npos )
    {
      vtkMRMLLinearTransformNode* currentTransform = StringToTransformNode( value );
      std::stringstream transformName;
      transformName << frameFieldName.c_str() << "_" << std::setw( 5 ) << std::setfill( '0' ) << frameNumber;
      currentTransform->SetName( transformName.str().c_str() );
      currentTransform->SetScene( this->GetMRMLScene() );

      char buffer[256];
      this->MultiDimensionLogic->AddChildNodeAtTimePoint( rootNode, currentTransform, itoa( frameNumber, buffer, 10 ) );
      this->GetMRMLScene()->AddNode( currentTransform );
    }

    if ( frameFieldName.find( "Timestamp" ) != std::string::npos )
    {
      std::stringstream frameStream;
      frameStream << frameNumber;
      this->frameToTimeMap[ frameStream.str() ] = value;
      std::string testValue = this->frameToTimeMap[ frameStream.str() ];
      std::string test;
    }

    if ( ferror( stream ) )
    {
      // vtkErrorMacro("Error reading the file "<<this->FileName);
      break;
    }
    if ( feof( stream ) )
    {
      break;
    }

  }

  fclose( stream );
}

//----------------------------------------------------------------------------
// Read the spacing and dimentions of the image.
void vtkSlicerMetafileImporterLogic
::ReadImages( std::string fileName )
{
  // Grab the image data from the mha file
  vtkSmartPointer< vtkMetaImageReader > imageReader = vtkSmartPointer< vtkMetaImageReader >::New();
  imageReader->SetFileName( fileName.c_str() );
  imageReader->Update();

  vtkSmartPointer< vtkImageData > imageData = imageReader->GetOutput();
  int* dimensions = imageData->GetDimensions();
  double* spacing = imageData->GetSpacing();

  vtkSmartPointer< vtkMRMLScalarVolumeNode > allVolumesNode = vtkSmartPointer< vtkMRMLScalarVolumeNode >::New();
  allVolumesNode->SetAndObserveImageData( imageData );

  // Now, read the volume into individual image slices
  // Assume the slices are z-planes
  // Observe that the VOIs are zero-offset
  for ( int i = 0; i < dimensions[2]; i++ )
  {
    vtkSmartPointer< vtkExtractVOI > imageSlicer = vtkSmartPointer< vtkExtractVOI >::New();
    imageSlicer->SetInput( imageData );
    imageSlicer->SetVOI( 0, dimensions[0] - 1, 0, dimensions[1] - 1, i, i );
    imageSlicer->Update();

    vtkSmartPointer< vtkMRMLScalarVolumeDisplayNode > displayNode = vtkSmartPointer< vtkMRMLScalarVolumeDisplayNode >::New();

    vtkSmartPointer< vtkMRMLScalarVolumeNode > slice = vtkSmartPointer< vtkMRMLScalarVolumeNode >::New();
    slice->SetAndObserveDisplayNodeID( displayNode->GetID() );
    slice->SetAndObserveImageData( imageSlicer->GetOutput() );
    std::stringstream sliceName;
    sliceName << "Image_" << std::setw( 5 ) << std::setfill( '0' ) << i;
    slice->SetName( sliceName.str().c_str() );
    slice->SetScene( this->GetMRMLScene() );

    // Move the image slice so it is at the origin
    vtkSmartPointer< vtkMatrix4x4 > sliceTransformMatrix = vtkSmartPointer< vtkMatrix4x4 >::New();
    sliceTransformMatrix->Identity();
    sliceTransformMatrix->SetElement( 2, 3, - i * spacing[2] );
    slice->ApplyTransformMatrix( sliceTransformMatrix );

    char buffer[256];
    this->MultiDimensionLogic->AddChildNodeAtTimePoint( rootNode, slice, itoa( i, buffer, 10 ) );
    this->GetMRMLScene()->AddNode( slice );
  }

}




//----------------------------------------------------------------------------
void vtkSlicerMetafileImporterLogic
::Read( std::string fileName )
{
  // Setup hierarchy structure
  this->rootNode = this->MultiDimensionLogic->CreateMultiDimensionRootNode();
  int dotFound = fileName.find_last_of( "." );
  int slashFound = fileName.find_last_of( "/" );
  std::stringstream rootName;
  rootName << this->rootNode->GetName() << "_" << fileName.substr( slashFound + 1, dotFound - slashFound - 1 );
  this->rootNode->SetName( rootName.str().c_str() );

  this->ReadTransforms( fileName ); // TODO: Removed error macro
  this->ReadImages( fileName ); // TODO: Removed error macro

  this->MultiDimensionLogic->UpdateValues( this->rootNode, this->frameToTimeMap );
}

 
