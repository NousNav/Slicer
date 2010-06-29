/*=========================================================================

  Program:   Visualization Toolkit
  Module:    $RCSfile: vtkDisplayableManagerInteractorStyle.cxx,v $

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkDisplayableManagerInteractorStyle.h"

#include "vtkCamera.h"
#include "vtkCommand.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkMRMLSelectionNode.h"
#include "vtkMRMLInteractionNode.h"

// for picking
#include "vtkMRMLModelDisplayableManager.h"

vtkCxxRevisionMacro(vtkDisplayableManagerInteractorStyle, "$Revision: 13328 $");
vtkStandardNewMacro(vtkDisplayableManagerInteractorStyle);

//----------------------------------------------------------------------------
vtkDisplayableManagerInteractorStyle::vtkDisplayableManagerInteractorStyle() 
{
  this->MotionFactor   = 10.0;
  this->CameraNode = NULL;
  this->NumberOfPicks = 0;
  this->NumberOfPlaces= 0;
  this->NumberOfTransientPicks = 1;
  this->NumberOfTransientPlaces = 1;
}

//----------------------------------------------------------------------------
vtkDisplayableManagerInteractorStyle::~vtkDisplayableManagerInteractorStyle() 
{
  this->SetCameraNode(NULL);
  this->NumberOfPicks = 0;
  this->NumberOfPlaces= 0;
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnMouseMove() 
{ 
  int x = this->Interactor->GetEventPosition()[0];
  int y = this->Interactor->GetEventPosition()[1];
  int cameraDisableState = 0;

  if (this->CameraNode) 
    {
    cameraDisableState = this->CameraNode->GetDisableModifiedEvent();
    this->CameraNode->DisableModifiedEventOn();
    }

  switch (this->State) 
    {
    case VTKIS_ROTATE:
      this->FindPokedRenderer(x, y);
      this->Rotate();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;

    case VTKIS_PAN:
      this->FindPokedRenderer(x, y);
      this->Pan();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;

    case VTKIS_DOLLY:
      this->FindPokedRenderer(x, y);
      this->Dolly();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;

    case VTKIS_SPIN:
      this->FindPokedRenderer(x, y);
      this->Spin();
      this->InvokeEvent(vtkCommand::InteractionEvent, NULL);
      break;
    default:
      this->InvokeEvent(vtkCommand::MouseMoveEvent, NULL);
      break;
    }

  if (this->CameraNode) 
    {
    this->CameraNode->SetDisableModifiedEvent(cameraDisableState);
    this->CameraNode->InvokePendingModifiedEvent();
    }
}


//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnEnter()
{
  if (this->HasObserver(vtkCommand::EnterEvent)) 
    {
    this->InvokeEvent(vtkCommand::EnterEvent,NULL);
    }
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnLeave()
{
  if (this->HasObserver(vtkCommand::LeaveEvent)) 
    {
    this->InvokeEvent(vtkCommand::LeaveEvent,NULL);
    }
}



//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnLeftButtonDown() 
{ 
  this->FindPokedRenderer(this->Interactor->GetEventPosition()[0], 
                          this->Interactor->GetEventPosition()[1]);
  if (this->CurrentRenderer == NULL)
    {
    vtkDebugMacro("OnLeftButtonDown: couldn't find the poked renderer at event position " << this->Interactor->GetEventPosition()[0] << ", " << this->Interactor->GetEventPosition()[1]);
    return;
    }
  
  // get the scene's mouse interaction mode
  int mouseInteractionMode = vtkMRMLInteractionNode::ViewTransform;
  int pickModePersistence = 0;
  int placeModePersistence = 0;
  vtkMRMLInteractionNode *interactionNode = NULL;
    
  if ( this->GetCameraNode() != NULL )
    {
    interactionNode = vtkMRMLInteractionNode::SafeDownCast (this->GetCameraNode()->GetScene()->GetNthNodeByClass(0,"vtkMRMLInteractionNode"));

    if (interactionNode != NULL)
      {
      mouseInteractionMode = interactionNode->GetCurrentInteractionMode();
      pickModePersistence = interactionNode->GetPickModePersistence();
      placeModePersistence = interactionNode->GetPlaceModePersistence();
      vtkDebugMacro("OnLeftButtonDown: mouse interaction mode = " << mouseInteractionMode);
      }
    else
      {
      vtkErrorMacro("OnLeftButtonDown: no interaction node! Assuming ViewTransform");
      }
    }
  
  
  
  if (this->Interactor->GetShiftKey()) 
    {
    if (this->Interactor->GetControlKey()) 
      {
      this->StartDolly();
      }
    else 
      {
      this->StartPan();
      }
    } 
  else 
    {
    if (this->Interactor->GetControlKey()) 
      {
      this->StartSpin();
      }
    else 
      {
      if (mouseInteractionMode == vtkMRMLInteractionNode::ViewTransform)
        {
        this->StartRotate();
        }
      else if (mouseInteractionMode == vtkMRMLInteractionNode::Place ||
               mouseInteractionMode == vtkMRMLInteractionNode::PickManipulate)
        {
        // get the current event position, flipping Y
        int x = this->Interactor->GetEventPosition()[0];
        int rawY = this->Interactor->GetEventPosition()[1];
        this->Interactor->SetEventPositionFlipY(x, rawY);
        int y = this->Interactor->GetEventPosition()[1];
        vtkDebugMacro("MouseMode Place or PickManipulate:: got x = " << x << ", y = " << y << " (raw y = " << rawY << ")\n");
        // now throw the events
        if (mouseInteractionMode == vtkMRMLInteractionNode::Place)
          {
          this->InvokeEvent(vtkDisplayableManagerInteractorStyle::PickEvent, this->Interactor->GetEventPosition());
          //--- increment the number of Places that have occured.
          this->NumberOfPlaces++;
          }
        else if (mouseInteractionMode == vtkMRMLInteractionNode::PickManipulate)
          {
          // deal with select mode
          // throw a select region event
          this->InvokeEvent(vtkDisplayableManagerInteractorStyle::SelectRegionEvent, this->Interactor->GetEventPosition());
          this->NumberOfPicks++;
          // TODO: expand the mouse interaction modes and events to support
          // picking everything needed
#ifndef QDEC_DEBUG
          // for the Qdec module, throw a plot event that won't clash with the
          // fiducials module looking for a pick event
          this->InvokeEvent(vtkDisplayableManagerInteractorStyle::PlotEvent, this->Interactor->GetEventPosition());
#endif
          }
        }
      }
    }

  if ( interactionNode != NULL )
    {
      // release the pointer
    interactionNode = NULL;
    }
  
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnLeftButtonUp()
{
  // get the scene's mouse interaction mode
  int mouseInteractionMode = vtkMRMLInteractionNode::ViewTransform;
  int pickModePersistence = 0;
  int placeModePersistence = 0;
  vtkMRMLInteractionNode *interactionNode = NULL;
    
  if ( this->GetCameraNode() != NULL )
    {
    interactionNode = vtkMRMLInteractionNode::SafeDownCast (this->GetCameraNode()->GetScene()->GetNthNodeByClass(0,"vtkMRMLInteractionNode"));

    if (interactionNode != NULL)
      {
      mouseInteractionMode = interactionNode->GetCurrentInteractionMode();
      pickModePersistence = interactionNode->GetPickModePersistence();
      placeModePersistence = interactionNode->GetPlaceModePersistence();
      vtkDebugMacro("OnLeftButtonUp: mouse interaction mode = " << mouseInteractionMode);
      }
    else
      {
      vtkErrorMacro("OnLeftButtonUp: no interaction node! Assuming ViewTransform");
      }
    }

  // now throw the events if mouse mode is transient.
  if (mouseInteractionMode == vtkMRMLInteractionNode::Place)
    {
    //--- count the number of picks and
    //--- drop the interaction mode back to 
    //--- the default (transform) if mouse mode
    //--- is transient.
    if ( (this->GetNumberOfPlaces() >= this->GetNumberOfTransientPlaces() ) &&
         (placeModePersistence == 0 ) && (interactionNode != NULL) )
      {
      interactionNode->NormalizeAllMouseModes();
      interactionNode->SetLastInteractionMode ( mouseInteractionMode );
      interactionNode->SetCurrentInteractionMode ( vtkMRMLInteractionNode::ViewTransform );
      // reset the number of place events.
      this->NumberOfPlaces = 0;
      interactionNode->InvokeEvent ( vtkMRMLInteractionNode::TransientTimeoutEvent);
      }
    }
  else if (mouseInteractionMode == vtkMRMLInteractionNode::PickManipulate)
    {
    // deal with select mode
    //--- count the number of picks and
    //--- drop the interaction mode back to 
    //--- the default (transform) if mousemode 
    //--- is transient.
    if ( (this->GetNumberOfPicks() >= this->GetNumberOfTransientPicks() ) &&
         (pickModePersistence == 0  ) && (interactionNode != NULL) )
      {
      interactionNode->NormalizeAllMouseModes();
      interactionNode->SetLastInteractionMode ( mouseInteractionMode );
      interactionNode->SetCurrentInteractionMode ( vtkMRMLInteractionNode::ViewTransform );
      // reset the number of pick events.
      this->NumberOfPicks = 0;
      interactionNode->InvokeEvent ( vtkMRMLInteractionNode::TransientTimeoutEvent);
      }
    }
  switch (this->State) 
    {
    case VTKIS_DOLLY:
      this->EndDolly();
      break;

    case VTKIS_PAN:
      this->EndPan();
      break;

    case VTKIS_SPIN:
      this->EndSpin();
      break;

    case VTKIS_ROTATE:
      this->EndRotate();
      break;
    }

}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnMiddleButtonDown() 
{
  this->FindPokedRenderer(this->Interactor->GetEventPosition()[0], 
                          this->Interactor->GetEventPosition()[1]);
  if (this->CurrentRenderer == NULL)
    {
    return;
    }
  
  this->StartPan();
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnMiddleButtonUp()
{
  switch (this->State) 
    {
    case VTKIS_PAN:
      this->EndPan();
      break;
    }
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnRightButtonDown() 
{
  this->FindPokedRenderer(this->Interactor->GetEventPosition()[0], 
                          this->Interactor->GetEventPosition()[1]);
  if (this->CurrentRenderer == NULL)
    {
    return;
    }
  
  this->StartDolly();
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnRightButtonUp()
{
  switch (this->State) 
    {
    case VTKIS_DOLLY:
      this->EndDolly();
      break;
    }
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnMouseWheelForward() 
{
  this->FindPokedRenderer(this->Interactor->GetEventPosition()[0], 
                          this->Interactor->GetEventPosition()[1]);
  if (this->CurrentRenderer == NULL)
    {
    return;
    }
  
  this->StartDolly();
  double factor = this->MotionFactor * 0.2 * this->MouseWheelMotionFactor;
  // Slicer; invert direction to match right button drag
  this->Dolly(pow((double)1.1, -1. * factor));
  this->EndDolly();
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnMouseWheelBackward()
{
  this->FindPokedRenderer(this->Interactor->GetEventPosition()[0], 
                          this->Interactor->GetEventPosition()[1]);
  if (this->CurrentRenderer == NULL)
    {
    return;
    }
  
  this->StartDolly();
  double factor = this->MotionFactor * -0.2 * this->MouseWheelMotionFactor;
  // Slicer; invert direction to match right button drag
  this->Dolly(pow((double)1.1, -1. * factor));
  this->EndDolly();
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::OnExpose()
{
  if ( this->GetViewerWidget() != NULL )
    {
    this->GetViewerWidget()->RequestRender();
    }

}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::Rotate()
{
  if (this->CurrentRenderer == NULL)
    {
    return;
    }
  
  vtkRenderWindowInteractor *rwi = this->Interactor;

  int dx = rwi->GetEventPosition()[0] - rwi->GetLastEventPosition()[0];
  int dy = rwi->GetEventPosition()[1] - rwi->GetLastEventPosition()[1];
  
  int *size = this->CurrentRenderer->GetRenderWindow()->GetSize();

  double delta_elevation = -20.0 / size[1];
  double delta_azimuth = -20.0 / size[0];
  
  double rxf = (double)dx * delta_azimuth * this->MotionFactor;
  double ryf = (double)dy * delta_elevation * this->MotionFactor;
  
  vtkCamera *camera = NULL;
  if (this->CameraNode)
    {
    camera = this->CameraNode->GetCamera();
    }
  else
    {
    camera = this->CurrentRenderer->IsActiveCameraCreated() ? this->CurrentRenderer->GetActiveCamera() : NULL;
    }

  if (!camera)
    {
    return;
    }

  camera->Azimuth(rxf);
  camera->Elevation(ryf);
  camera->OrthogonalizeViewUp();

  if (this->AutoAdjustCameraClippingRange)
    {
    this->CurrentRenderer->ResetCameraClippingRange();
    }

  if (rwi->GetLightFollowCamera()) 
    {
    this->CurrentRenderer->UpdateLightsGeometryToFollowCamera();
    }

  // release the camera
  camera = NULL;
  //rwi->Render();
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::Spin()
{
  if (this->CurrentRenderer == NULL)
    {
    return;
    }

  vtkRenderWindowInteractor *rwi = this->Interactor;

  double *center = this->CurrentRenderer->GetCenter();

  double newAngle = 
    atan2((double)rwi->GetEventPosition()[1] - (double)center[1],
          (double)rwi->GetEventPosition()[0] - (double)center[0]);

  double oldAngle = 
    atan2((double)rwi->GetLastEventPosition()[1] - (double)center[1],
          (double)rwi->GetLastEventPosition()[0] - (double)center[0]);

#if ( (VTK_MAJOR_VERSION >= 6) || ( VTK_MAJOR_VERSION == 5 && VTK_MINOR_VERSION >= 4 ) )
  newAngle = vtkMath::DegreesFromRadians(newAngle);
  oldAngle = vtkMath::DegreesFromRadians(oldAngle);
#else
  newAngle *= vtkMath::RadiansToDegrees();
  oldAngle *= vtkMath::RadiansToDegrees();
#endif
  vtkCamera *camera = NULL;
  if (this->CameraNode)
    {
    camera = this->CameraNode->GetCamera();
    }
  else
    {
    camera = this->CurrentRenderer->IsActiveCameraCreated() ? this->CurrentRenderer->GetActiveCamera() : NULL;
    }

  if (!camera)
    {
    return;
    }

  camera->Roll(newAngle - oldAngle);
  camera->OrthogonalizeViewUp();

  // release the camera
  camera = NULL;
  
  rwi->Render();
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::Pan()
{
  if (this->CurrentRenderer == NULL)
    {
    return;
    }

  vtkRenderWindowInteractor *rwi = this->Interactor;

  double viewFocus[4], focalDepth, viewPoint[3];
  double newPickPoint[4], oldPickPoint[4], motionVector[3];
  
  // Calculate the focal depth since we'll be using it a lot
  vtkCamera *camera = NULL;
  if (this->CameraNode)
    {
    camera = this->CameraNode->GetCamera();
    }
  else
    {
    camera = this->CurrentRenderer->IsActiveCameraCreated() ? this->CurrentRenderer->GetActiveCamera() : NULL;
    }

  if (!camera)
    {
    return;
    }

  camera->GetFocalPoint(viewFocus);
  this->ComputeWorldToDisplay(viewFocus[0], viewFocus[1], viewFocus[2], 
                              viewFocus);
  focalDepth = viewFocus[2];

  this->ComputeDisplayToWorld((double)rwi->GetEventPosition()[0], 
                              (double)rwi->GetEventPosition()[1],
                              focalDepth, 
                              newPickPoint);
    
  // Has to recalc old mouse point since the viewport has moved,
  // so can't move it outside the loop

  this->ComputeDisplayToWorld((double)rwi->GetLastEventPosition()[0],
                              (double)rwi->GetLastEventPosition()[1],
                              focalDepth, 
                              oldPickPoint);
  
  // Camera motion is reversed

  motionVector[0] = oldPickPoint[0] - newPickPoint[0];
  motionVector[1] = oldPickPoint[1] - newPickPoint[1];
  motionVector[2] = oldPickPoint[2] - newPickPoint[2];
  
  camera->GetFocalPoint(viewFocus);
  camera->GetPosition(viewPoint);
  camera->SetFocalPoint(motionVector[0] + viewFocus[0],
                        motionVector[1] + viewFocus[1],
                        motionVector[2] + viewFocus[2]);

  camera->SetPosition(motionVector[0] + viewPoint[0],
                      motionVector[1] + viewPoint[1],
                      motionVector[2] + viewPoint[2]);
      
  if (rwi->GetLightFollowCamera()) 
    {
    this->CurrentRenderer->UpdateLightsGeometryToFollowCamera();
    }

  // release the camera
  camera = NULL;
  
  rwi->Render();
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::Dolly()
{
  if (this->CurrentRenderer == NULL)
    {
    return;
    }
  
  vtkRenderWindowInteractor *rwi = this->Interactor;
  double *center = this->CurrentRenderer->GetCenter();
  int dy = rwi->GetEventPosition()[1] - rwi->GetLastEventPosition()[1];
  double dyf = this->MotionFactor * (double)(dy) / (double)(center[1]);
  // Slicer: pull mouse towards you to bring models closer (opposite of vtk)
  this->Dolly(pow((double)1.1, -1. * dyf)); 
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::Dolly(double factor)
{
  if (this->CurrentRenderer == NULL)
    {
    return;
    }
  
  vtkCamera *camera = NULL;
  if (this->CameraNode)
    {
    camera = this->CameraNode->GetCamera();
    }
  else
    {
    camera = this->CurrentRenderer->IsActiveCameraCreated() ? this->CurrentRenderer->GetActiveCamera() : NULL;
    }

  if (!camera)
    {
    return;
    }

  if (camera->GetParallelProjection())
    {
    camera->SetParallelScale(camera->GetParallelScale() / factor);
    }
  else
    {
    camera->Dolly(factor);
    if (this->AutoAdjustCameraClippingRange)
      {
      this->CurrentRenderer->ResetCameraClippingRange();
      }
    }
  
  if (this->Interactor->GetLightFollowCamera()) 
    {
    this->CurrentRenderer->UpdateLightsGeometryToFollowCamera();
    }
  
  this->Interactor->Render();

  camera = NULL;
}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

}

//----------------------------------------------------------------------------
void vtkDisplayableManagerInteractorStyle::SetViewerWidget ( vtkMRMLModelDisplayableManager *viewerWidget )
{
  this->ViewerWidget = viewerWidget;
}

