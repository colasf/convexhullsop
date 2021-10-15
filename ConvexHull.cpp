/* Shared Use License: This file is owned by Derivative Inc. (Derivative)
* and can only be used, and/or modified for use, in conjunction with
* Derivative's TouchDesigner software, and only if you are a licensee who has
* accepted Derivative's TouchDesigner license or assignment agreement
* (which also govern the use of this file). You may share or redistribute
* a modified version of this file provided the following conditions are met:
*
* 1. The shared file or redistribution must retain the information set out
* above and this list of conditions.
* 2. Derivative's name (Derivative Inc.) or its trademarks may not be used
* to endorse or promote products derived from this file without specific
* prior written permission from Derivative.
*/

#include "ConvexHull.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// These functions are basic C function, which the DLL loader can find
// much easier than finding a C++ Class.
// The DLLEXPORT prefix is needed so the compile exports these functions from the .dll
// you are creating
extern "C"
{

	DLLEXPORT
	void
	FillSOPPluginInfo(SOP_PluginInfo *info)
	{
		// Always return SOP_CPLUSPLUS_API_VERSION in this function.
		info->apiVersion = SOPCPlusPlusAPIVersion;

		// The opType is the unique name for this TOP. It must start with a 
		// capital A-Z character, and all the following characters must lower case
		// or numbers (a-z, 0-9)
		info->customOPInfo.opType->setString("Convexhull");

		// The opLabel is the text that will show up in the OP Create Dialog
		info->customOPInfo.opLabel->setString("Convex Hull");

		// Will be turned into a 3 letter icon on the nodes
		info->customOPInfo.opIcon->setString("CXH");

		// Information about the author of this OP
		info->customOPInfo.authorName->setString("colas fiszman");
		info->customOPInfo.authorEmail->setString("colas.fiszman@gmail.com");

		// This SOP works with 0 or 1 inputs
		info->customOPInfo.minInputs = 1;
		info->customOPInfo.maxInputs = 1;

	}

	DLLEXPORT
	SOP_CPlusPlusBase*
	CreateSOPInstance(const OP_NodeInfo* info)
	{
		// Return a new instance of your class every time this is called.
		// It will be called once per SOP that is using the .dll
		return new ConvexHull(info);
	}

	DLLEXPORT
	void
	DestroySOPInstance(SOP_CPlusPlusBase* instance)
	{
		// Delete the instance here, this will be called when
		// Touch is shutting down, when the SOP using that instance is deleted, or
		// if the SOP loads a different DLL
		delete (ConvexHull*)instance;
	}

};


ConvexHull::ConvexHull(const OP_NodeInfo* info) : myNodeInfo(info)
{

}

ConvexHull::~ConvexHull()
{
	 
}

void
ConvexHull::getGeneralInfo(SOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = true;

	//if direct to GPU loading:
	ginfo->directToGPU = false;
}

void
ConvexHull::execute(SOP_Output* output, const OP_Inputs* inputs, void* reserved)
{

	if (inputs->getNumInputs() > 0)
	{

		// get the first input
		const OP_SOPInput	*sinput = inputs->getInputSOP(0);

		// get the position of the points from the sop connected to the first input
		const Position* ptArr = sinput->getPointPositions();

		// convert the position;s pointer to a float pointer as that's what
		// the getConvexHull function need
		const float* positions = reinterpret_cast<const float*>(ptArr);

		// get epsilon value
		float epsilon = static_cast<float>(inputs->getParDouble("Epsilon"));

		// get triangle vertex order
		bool ccw = static_cast<bool>(inputs->getParInt("Ccw"));

		// generate the convex hull
	    quickhull::ConvexHull<float> hull = qh.getConvexHull(positions,
		                                                    sinput->getNumPoints(),
			                                                ccw,
		                                                    false,
			                                                epsilon);

		
		// get the points from the convexHull geo and add them to the SOP
		const auto& vertexBuffer = hull.getVertexBuffer();
		
		for (auto& point : vertexBuffer) {
			output->addPoint(Position(point.x, point.y, point.z));
		}

		// get the indexex from the convexHull and create the triangles on the sop
		const auto& indexBuffer = hull.getIndexBuffer();

		for (size_t i = 0; i < indexBuffer.size() / 3; i++)
		{

			size_t triangleFirstIndice = i * 3;

			// Note: the addTriangle() assumes that the input SOP has triangulated geometry,
			// if the input geometry is not a triangle, you need to convert it to triangles first:
			output->addTriangle(indexBuffer[triangleFirstIndice],
				                indexBuffer[triangleFirstIndice+1],
				                indexBuffer[triangleFirstIndice+2]);
		}

	}
	
}



void
ConvexHull::executeVBO(SOP_VBOOutput* output,
						const OP_Inputs* inputs,
						void* reserved)
{
	return;
}

//-----------------------------------------------------------------------------------------------------
//								CHOP, DAT, and custom parameters
//-----------------------------------------------------------------------------------------------------

int32_t
ConvexHull::getNumInfoCHOPChans(void* reserved)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send 4 channels.
	return 0;
}

void
ConvexHull::getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan* chan, void* reserved)
{
	
}

bool
ConvexHull::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved)
{
	infoSize->rows = 0;
	infoSize->cols = 0;
	// Setting this to false means we'll be assigning values to the table
	// one row at a time. True means we'll do it one column at a time.
	infoSize->byColumn = false;
	return true;
}

void
ConvexHull::getInfoDATEntries(int32_t index,
								int32_t nEntries,
								OP_InfoDATEntries* entries,
								void* reserved)
{

}



void
ConvexHull::setupParameters(OP_ParameterManager* manager, void* reserved)
{
	// Epsilon
	{
		OP_NumericParameter	np;

		np.name = "Epsilon";
		np.label = "Epsilon";
		np.defaultValues[0] = 0.0001f;
		np.minSliders[0] = 0.000001f;
		np.maxSliders[0] = 1.0;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// CCW
	{
		OP_NumericParameter	np;

		np.name = "Ccw";
		np.label = "Ccw";

		OP_ParAppendResult res = manager->appendToggle(np);
		assert(res == OP_ParAppendResult::Success);
	}

}

void
ConvexHull::pulsePressed(const char* name, void* reserved)
{

}

