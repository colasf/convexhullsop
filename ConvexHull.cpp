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
		info->customOPInfo.opType->setString("Simpleshapes");

		// The opLabel is the text that will show up in the OP Create Dialog
		info->customOPInfo.opLabel->setString("Simple Shapes");

		// Will be turned into a 3 letter icon on the nodes
		info->customOPInfo.opIcon->setString("SSP");

		// Information about the author of this OP
		info->customOPInfo.authorName->setString("Author Name");
		info->customOPInfo.authorEmail->setString("email@email.com");

		// This SOP works with 0 or 1 inputs
		info->customOPInfo.minInputs = 0;
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
	myExecuteCount = 0;
	myOffset = 0.0;
	myChop = "";

	myChopChanName = "";
	myChopChanVal = 0;

	myDat = "N/A";
}

ConvexHull::~ConvexHull()
{

}

void
ConvexHull::getGeneralInfo(SOP_GeneralInfo* ginfo, const OP_Inputs* inputs, void* reserved)
{
	// This will cause the node to cook every frame
	ginfo->cookEveryFrameIfAsked = false;

	//if direct to GPU loading:
	bool directGPU = inputs->getParInt("Gpudirect") != 0 ? true : false;
	ginfo->directToGPU = directGPU;

}


//-----------------------------------------------------------------------------------------------------
//										Generate a geometry on CPU
//-----------------------------------------------------------------------------------------------------

void
ConvexHull::cubeGeometry(SOP_Output* output, float scale)
{
	// to generate a geometry:
	// addPoint() is the first function to be called.
	// then we can add normals, colors, and any custom attributes for the points
	// last function can be either addParticleSystem() or addTriangle()

	// front
	output->addPoint(Position(1.0f*scale, -1.0f, 1.0f));
	output->addPoint(Position(3.0f*scale, -1.0f, 1.0f));
	output->addPoint(Position(3.0f*scale, 1.0f, 1.0f));
	output->addPoint(Position(1.0f*scale, 1.0, 1.0));
	// back
	output->addPoint(Position(1.0f*scale, -1.0f, -1.0f));
	output->addPoint(Position(3.0f*scale, -1.0f, -1.0f));
	output->addPoint(Position(3.0f*scale, 1.0f, -1.0f));
	output->addPoint(Position(1.0f*scale, 1.0f, -1.0f));

	Vector normal[] = {
		// front
		Vector(1.0, 0.0, 0.0),
		Vector(0.0, 1.0, 0.0),
		Vector(0.0, 0.0, 1.0),
		Vector(1.0, 1.0, 1.0),
		// back
		Vector(1.0, 0.0, 0.0),
		Vector(0.0, 1.0, 0.0),
		Vector(0.0, 0.0, 1.0),
		Vector(1.0, 1.0, 1.0),
	};

	Color color[] =
	{
		// front colors
		Color(1.0, 0.0, 0.0, 1.0),
		Color(0.0, 1.0, 0.0, 1.0),
		Color(0.0, 0.0, 1.0, 1.0),
		Color(1.0, 1.0, 1.0, 1.0),
		// back colors
		Color(1.0, 0.0, 0.0, 1.0),
		Color(0.0, 1.0, 0.0, 1.0),
		Color(0.0, 0.0, 1.0, 1.0),
		Color(1.0, 1.0, 1.0, 1.0),
	};

	float color2[] =
	{
		// front colors
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		// back colors
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
	};

	// indices of the input vertices

	int32_t vertices[] = {
		// front
		0, 1, 2,
		2, 3, 0,
		// top
		1, 5, 6,
		6, 2, 1,
		// back
		7, 6, 5,
		5, 4, 7,
		// bottom
		4, 0, 3,
		3, 7, 4,
		// left
		4, 5, 1,
		1, 0, 4,
		// right
		3, 2, 6,
		6, 7, 3,
	};

	int sz = 8;


	for (int32_t i = 0; i < sz; ++i)
	{
		output->setNormal(normal[i], i);
		output->setColor(color[i], i);
	}

	SOP_CustomAttribData cu;
	cu.name = "CustomColor";
	cu.attribType = AttribType::Float;
	cu.floatData = color2;
	cu.numComponents = 4;
	output->setCustomAttribute(&cu, sz);

	for (int i = 0; i < 12; i++)
	{
		output->addTriangle(vertices[i * 3],
							vertices[i * 3 + 1],
							vertices[i * 3 + 2]);
	}
}

void
ConvexHull::lineGeometry(SOP_Output* output)
{
	// to generate a geometry:
	// addPoint() is the first function to be called.
	// then we can add normals, colors, and any custom attributes for the points
	// last function to be called is addLines()

	// line 1 = 9 vertices
	output->addPoint(Position(-0.8f, 0.0f, 1.0f));
	output->addPoint(Position(-0.6f, 0.4f, 1.0f));
	output->addPoint(Position(-0.4f, 0.8f, 1.0f));
	output->addPoint(Position(-0.2f, 0.4f, 1.0f));
	output->addPoint(Position(0.0f,  0.0f, 1.0f));
	output->addPoint(Position(0.2f, -0.4f, 1.0f));
	output->addPoint(Position(0.4f, -0.8f, 1.0f));
	output->addPoint(Position(0.6f, -0.4f, 1.0f));
	output->addPoint(Position(0.8f,  0.0f, 1.0f));

	// line 2 = 8 vertices
	output->addPoint(Position(-0.8f, 0.2f, 1.0f));
	output->addPoint(Position(-0.6f, 0.6f, 1.0f));
	output->addPoint(Position(-0.4f, 1.0f, 1.0f));
	output->addPoint(Position(-0.2f, 0.6f, 1.0f));
	output->addPoint(Position(0.0f,  0.2f, 1.0f));
	output->addPoint(Position(0.2f, -0.2f, 1.0f));
	output->addPoint(Position(0.4f, -0.6f, 1.0f));
	output->addPoint(Position(0.6f, -0.2f, 1.0f));

	Vector normal[] =
	{
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),

		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
	};

	Color color[] =
	{
		Color(1.0f, 0.0f, 0.0f, 1.0f),
		Color(0.0f, 1.0f, 0.0f, 1.0f),
		Color(0.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 0.0f, 0.0f, 1.0f),
		Color(0.0f, 1.0f, 0.0f, 1.0f),
		Color(0.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 0.0f, 0.0f, 1.0f),

		Color(1.0f, 0.0f, 0.0f, 1.0f),
		Color(0.0f, 1.0f, 0.0f, 1.0f),
		Color(0.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 0.0f, 0.0f, 1.0f),
		Color(0.0f, 1.0f, 0.0f, 1.0f),
		Color(0.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
	};

	float color2[] =
	{
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,

		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
		1.0, 0.0, 0.0, 1.0,
	};

	// indices of the input vertices

	int32_t vertices[] =
	{
		0, 1, 2, 3, 4, 5, 6, 7, 8,
		9, 10, 11, 12, 13, 14, 15, 16
	};

	int32_t lineSizes[]
	{
		9, 8
	};

	// the size of all vertices of all lines:
	int sz = 9 + 8;

	for (int32_t i = 0; i < sz; ++i)
	{
		output->setNormal(normal[i], i);
		output->setColor(color[i], i);
	}

	SOP_CustomAttribData cu;
	cu.name = "CustomColor";
	cu.attribType = AttribType::Float;
	cu.floatData = color2;
	cu.numComponents = 4;
	output->setCustomAttribute(&cu, sz);

	//output->addLine(vertices, sz);
	output->addLines(vertices, lineSizes, 2);
}

void
ConvexHull::triangleGeometry(SOP_Output* output)
{
	int32_t vertices[3] = { 0, 1, 2 };

	//int sz = 3;
	output->addPoint(Position(0.0f, 0.0f, 0.0f));
	output->addPoint(Position(0.0f, 2.0f, 0.0f));
	output->addPoint(Position(2.0f, 0.0f, 0.0f));

	Vector normal(0.0f, 0.0f, 1.0f);

	output->setNormal(normal, 0);
	output->setNormal(normal, 1);
	output->setNormal(normal, 2);

	output->addTriangle(vertices[0], vertices[1], vertices[2]);

}

void
ConvexHull::execute(SOP_Output* output, const OP_Inputs* inputs, void* reserved)
{
	myExecuteCount++;

	if (inputs->getNumInputs() > 0)
	{
		inputs->enablePar("Reset", 0);	// not used
		inputs->enablePar("Shape", 0);	// not used
		inputs->enablePar("Scale", 0);  // not used

		const OP_SOPInput	*sinput = inputs->getInputSOP(0);

		const Position* ptArr = sinput->getPointPositions();
		const Vector* normals = nullptr;
		const Color* colors = nullptr;
		const TexCoord* textures = nullptr;
		int32_t numTextures = 0;

		if (sinput->hasNormals())
		{
			normals = sinput->getNormals()->normals;
		}

		if (sinput->hasColors())
		{
			colors = sinput->getColors()->colors;
		}

		if (sinput->getTextures()->numTextureLayers)
		{
			textures = sinput->getTextures()->textures;
			numTextures = sinput->getTextures()->numTextureLayers;
		}

		for (int i = 0; i < sinput->getNumPoints(); i++)
		{
			output->addPoint(ptArr[i]);

			if (normals)
			{
				output->setNormal(normals[i], i);
			}

			if (colors)
			{
				output->setColor(colors[i], i);
			}

			if (textures)
			{
				//output->setTexCoord((float*)(textures + (i * numTextures * 3)), numTextures, i);
				output->setTexCoord(textures + (i * numTextures), numTextures, i);
			}

		}

		for (int i = 0; i < sinput->getNumCustomAttributes(); i++)
		{
			const SOP_CustomAttribData* customAttr = sinput->getCustomAttribute(i);

			if (customAttr->attribType == AttribType::Float)
			{
				output->setCustomAttribute(customAttr, sinput->getNumPoints());
			}
			else
			{
				output->setCustomAttribute(customAttr, sinput->getNumPoints());
			}
		}


		for (int i = 0; i < sinput->getNumPrimitives(); i++)
		{

			const SOP_PrimitiveInfo primInfo = sinput->getPrimitive(i);

			const int32_t* primVert = primInfo.pointIndices;

			// Note: the addTriangle() assumes that the input SOP has triangulated geometry,
			// if the input geometry is not a triangle, you need to convert it to triangles first:
			output->addTriangle(*(primVert), *(primVert + 1), *(primVert + 2));
		}

	}
	else
	{
		inputs->enablePar("Shape", 1);

		int shape = inputs->getParInt("Shape");

		inputs->enablePar("Scale", 1);
		double	 scale = inputs->getParDouble("Scale");

		// if there is a input chop parameter:
		const OP_CHOPInput	*cinput = inputs->getParCHOP("Chop");
		if (cinput)
		{
			int numSamples = cinput->numSamples;
			int ind = 0;
			myChopChanName = std::string(cinput->getChannelName(0));
			myChop = inputs->getParString("Chop");

			myChopChanVal = float(cinput->getChannelData(0)[ind]);
			scale = float(cinput->getChannelData(0)[ind] * scale);

		}

		// create the geometry and set the bounding box for exact homing:
		switch (shape)
		{
			case 0:		// cube
			{
				cubeGeometry(output, (float)scale);
				output->setBoundingBox(BoundingBox(1.0f, -1.0f, -1.0f, 3.0f, 1.0f, 1.0f));

				// Add Point and Primitive groups:
				int numPts = output->getNumPoints();
				int grPts = floor(numPts / 2);
				int numPr = output->getNumPrimitives();

				const char gr1[] = "pointGroup";
				const char gr2[] = "primGroup";

				output->addGroup(SOP_GroupType::Point, gr1);
				output->addGroup(SOP_GroupType::Primitive, gr2);

				for (int i = 0; i < grPts; i++)
				{
					output->addPointToGroup(i, gr1);
				}

				for (int i = 0; i < numPr; i++)
				{
					output->addPrimToGroup(i, gr2);
				}

				break;
			}
			case 1:		// triangle
			{
				triangleGeometry(output);
				break;
			}
			case 2:		// line
			{
				lineGeometry(output);
				break;
			}
			default:
			{
				cubeGeometry(output, (float)scale);
				output->setBoundingBox(BoundingBox(1.0f, -1.0f, -1.0f, 3.0f, 1.0f, 1.0f));
				break;
			}
		}

	}


}

//-----------------------------------------------------------------------------------------------------
//								Generate a geometry and load it straight to GPU (faster)
//-----------------------------------------------------------------------------------------------------

// fillFaceVBO() get the vertices, normals, colors, texcoords and triangles buffer pointers and then fills in the
// buffers with the input arguments and their sizes.
void
fillFaceVBO(SOP_VBOOutput* output,
	Position* inVert, Vector* inNormal, Color* inColor, TexCoord* inTexCoord, int32_t*  inIdx,
	int VertSz, int triSize, int numTexLayers,
	float scale = 1.0f)
{

	Position* vertOut = nullptr;
	if(inVert)
		vertOut = output->getPos();
	Vector* normalOut = nullptr;
	if (inNormal)
		normalOut = output->getNormals();
	Color* colorOut = nullptr;
	if(inColor)
		colorOut = output->getColors();

	TexCoord* texCoordOut = nullptr;
	if(inTexCoord)
		texCoordOut = output->getTexCoords();
	int32_t* indexBuffer = output->addTriangles(triSize);

	for (int i = 0; i < triSize; i++)
	{
		*(indexBuffer++) = inIdx[i * 3 + 0];
		*(indexBuffer++) = inIdx[i * 3 + 1];
		*(indexBuffer++) = inIdx[i * 3 + 2];
	}

	int k = 0;
	while (k < VertSz * 3)
	{
		*(vertOut++) = inVert[k] * scale;

		if (output->hasNormal())
		{
			*(normalOut++) = inNormal[k];
		}

		if (output->hasColor())
		{
			*(colorOut++) = inColor[k];
		}

		if (output->hasTexCoord())
		{
			for (int t = 0; t < numTexLayers +1; t++)
			{
				*(texCoordOut++) = inTexCoord[(k * (numTexLayers + 1) + t)];
			}
		}
		k++;
	}
}

// fillLineVBO() get the vertices, normals, colors, texcoords and triangles buffer pointers and then fills in the
// buffers with the input arguments and their sizes.
void
fillLineVBO(SOP_VBOOutput* output,
	Position* inVert, Vector* inNormal, Color* inColor, TexCoord* inTexCoord, int32_t*  inIdx,
	int vertSz, int lineSize, int numTexLayers)
{
	Position* vertOut = nullptr;
	if (inVert)
		vertOut = output->getPos();
	Vector* normalOut = nullptr;
	if (inNormal)
		normalOut = output->getNormals();
	Color* colorOut = nullptr;
	if (inColor)
		colorOut = output->getColors();
	TexCoord* texCoordOut = nullptr;
	if (inTexCoord)
		texCoordOut = output->getTexCoords();

	int32_t* indexBuffer = output->addLines(lineSize);

	for (int i = 0; i < lineSize; i++)
	{
		*(indexBuffer++) = inIdx[i];
	}

	int k = 0;
	while (k < vertSz)
	{
		*(vertOut++) = inVert[k];

		if (output->hasNormal())
		{
			*(normalOut++) = inNormal[k];
		}

		if (output->hasColor())
		{
			*(colorOut++) = inColor[k];
		}

		if (output->hasTexCoord())
		{
			for (int t = 0; t < numTexLayers + 1; t++)
			{
				*(texCoordOut++) = inTexCoord[(k * (numTexLayers + 1) + t)];
			}
		}
		k++;
	}
}

// fillFaceVBO() get the vertices, normals, colors, texcoords and triangles buffer pointers and then fills in the
// buffers with the input arguments and their sizes.
void
fillParticleVBO(SOP_VBOOutput* output,
	Position* inVert, Vector* inNormal, Color* inColor, TexCoord* inTexCoord, int32_t*  inIdx,
	int vertSz, int size, int numTexLayers)
{

	Position* vertOut = nullptr;
	if (inVert)
		vertOut = output->getPos();
	Vector* normalOut = nullptr;
	if (inNormal)
		normalOut = output->getNormals();
	Color* colorOut = nullptr;
	if (inColor)
		colorOut = output->getColors();
	TexCoord* texCoordOut = nullptr;
	if (inTexCoord)
		texCoordOut = output->getTexCoords();

	int32_t* indexBuffer = output->addParticleSystem(size);

	for (int i = 0; i < size; i++)
	{
		*(indexBuffer++) = inIdx[i];
	}

	int k = 0;
	while (k < vertSz)
	{
		*(vertOut++) = inVert[k];

		if (output->hasNormal())
		{
			*(normalOut++) = inNormal[k];
		}

		if (output->hasColor())
		{
			*(colorOut++) = inColor[k];
		}

		if (output->hasTexCoord())
		{
			for (int t = 0; t < numTexLayers + 1; t++)
			{
				*(texCoordOut++) = inTexCoord[(k * (numTexLayers + 1) + t)];
			}
		}
		k++;
	}
}

void
ConvexHull::cubeGeometryVBO(SOP_VBOOutput* output, float scale)
{
	Position pointArr[] =
	{
		//front
		Position(1.0, -1.0, 1.0), //v0
		Position(3.0, -1.0, 1.0), //v1
		Position(3.0, 1.0, 1.0),  //v2
		Position(1.0, 1.0, 1.0), //v3

		//right
		Position(3.0, 1.0, 1.0), //v2
		Position(3.0, 1.0, -1.0), //v6
		Position(3.0, -1.0, -1.0),//v5
		Position(3.0, -1.0, 1.0),//v1


		//back
		Position(1.0, -1.0, -1.0), //v4
		Position(3.0, -1.0, -1.0),  //v5
		Position(3.0, 1.0, -1.0),  //v6
		Position(1.0, 1.0, -1.0), //v7


		//left
		Position(1.0, -1.0, -1.0), //v4
		Position(1.0, -1.0, 1.0),// v0
		Position(1.0, 1.0, 1.0),//v3
		Position(1.0, 1.0, -1.0),//v7


		//upper
		Position(3.0, 1.0, 1.0),//v1
		Position(1.0, 1.0, 1.0),//v3
		Position(1.0, 1.0, -1.0),//v7
		Position(3.0, 1.0, -1.0),//v6


		//bottom
		Position(1.0, -1.0, -1.0),//v4
		Position(3.0, -1.0, -1.0),//v5
		Position(3.0, -1.0, 1.0),//v1
		Position(1.0, -1.0, 1.0)//v0
	};

	Vector normals[] =
	{
		//front
		Vector(1.0, 0.0, 0.0), //v0
		Vector(0.0, 1.0, 0.0),//v1
		Vector(0.0, 0.0, 1.0),//v2
		Vector(1.0, 1.0, 1.0),//v3

		//right
		Vector(0.0, 0.0, 1.0),//v2
		Vector(0.0, 0.0, 1.0), //v6
		Vector(0.0, 1.0, 0.0),//v5
		Vector(0.0, 1.0, 0.0),//v1

		//back
		Vector(1.0, 0.0, 0.0), //v4
		Vector(0.0, 1.0, 0.0), //v5
		Vector(0.0, 0.0, 1.0),//v6
		Vector(1.0, 1.0, 1.0),//v7

		//left
		Vector(1.0, 0.0, 0.0), //v4
		Vector(1.0, 0.0, 0.0),// v0
		Vector(1.0, 1.0, 1.0),//v3
		Vector(1.0, 1.0, 1.0),//v7

		//upper
		Vector(0.0, 1.0, 0.0),//v1
		Vector(1.0, 1.0, 1.0),//v3
		Vector(1.0, 1.0, 1.0),//v7
		Vector(0.0, 0.0, 1.0),//v6

		//bottom
		Vector(1.0, 0.0, 0.0),//v4
		Vector(0.0, 1.0, 0.0),//v5
		Vector(0.0, 1.0, 0.0),//v1
		Vector(1.0, 0.0, 0.0),//v0
	};

	Color colors[] = {
		//front
		Color(0, 0, 1, 1),
		Color(1, 0, 1, 1),
		Color(1, 1, 1, 1),
		Color(0, 1, 1, 1),

		//right
		Color(1, 1, 1, 1),
		Color(1, 1, 0, 1),
		Color(1, 0, 0, 1),
		Color(1, 0, 1, 1),

		//back
		Color(0, 0, 0, 1),
		Color(1, 0, 0, 1),
		Color(1, 1, 0, 1),
		Color(0, 1, 0, 1),

		//left
		Color(0, 0, 0, 1),
		Color(0, 0, 1, 1),
		Color(0, 1, 1, 1),
		Color(0, 1, 0, 1),

		//up
		Color(1, 1, 1, 1),
		Color(0, 1, 1, 1),
		Color(0, 1, 0, 1),
		Color(1, 1, 0, 1),

		//bottom
		Color(0, 0, 0, 1),
		Color(1, 0, 0, 1),
		Color(1, 0, 1, 1),
		Color(0, 0, 1, 1)
	};

	TexCoord texcoords[] =
	{
		//front
		TexCoord(0.0, 0.0, 0.0),//v0
		TexCoord(0.0, 1.0, 0.0),//v1
		TexCoord(1.0, 1.0, 0.0),//v2
		TexCoord(1.0, 0.0, 0.0),//v3

		//right
		TexCoord(1.0, 0.0, 0.0),//v2
		TexCoord(1.0, 1.0, 0.0),//v6
		TexCoord(1.0, 1.0, 0.0),//v5
		TexCoord(1.0, 0.0, 0.0),//v1


		//back
		TexCoord(1.0, 0.0, 0.0),//v4
		TexCoord(1.0, 1.0, 0.0),//v5
		TexCoord(0.0, 1.0, 0.0),//v6
		TexCoord(0.0, 0.0, 0.0),//v7


		//left
		TexCoord(0.0, 0.0, 0.0), //v4
		TexCoord(0.0, 1.0, 0.0), //v0
		TexCoord(0.0, 1.0, 0.0),//v3
		TexCoord(0.0, 0.0, 0.0),//v7


		//upper
		TexCoord(0.0, 0.0, 0.0),//v1
		TexCoord(0.0, 0.0, 0.0),//v3
		TexCoord(1.0, 0.0, 0.0),//v7
		TexCoord(1.0, 0.0, 0.0),//v6


		//bottom
		TexCoord(0.0, 0.0, 0.0),//v4
		TexCoord(0.0, 1.0, 0.0),//v5
		TexCoord(1.0, 1.0, 0.0),//v1
		TexCoord(1.0, 1.0, 0.0),//v0
	};

	int32_t vertices[] =
	{
		0,  1,  2,  0,  2,  3,   //front
		4,  5,  6,  4,  6,  7,   //right
		8,  9,  10, 8,  10, 11,  //back
		12, 13, 14, 12, 14, 15,  //left
		16, 17, 18, 16, 18, 19,  //upper
		20, 21, 22, 20, 22, 23
	};

	// fill in the VBO buffers for this cube:

	fillFaceVBO(output, pointArr, normals, colors, texcoords, vertices, 8, 12, myNumVBOTexLayers, scale);

	return;
}

void
ConvexHull::lineGeometryVBO(SOP_VBOOutput* output)
{
	Position pointArr[] =
	{
		Position(-0.8f, 0.0f, 1.0f),
		Position(-0.6f, 0.4f, 1.0f),
		Position(-0.4f, 0.8f, 1.0f),
		Position(-0.2f, 0.4f, 1.0f),
		Position(0.0f,  0.0f, 1.0f),
		Position(0.2f, -0.4f, 1.0f),
		Position(0.4f, -0.8f, 1.0f),
		Position(0.6f, -0.4f, 1.0f),
		Position(0.8f,  0.0f, 1.0f),
	};

	Vector normals[] =
	{
		Vector(1.0, 1.0, 1.0),
		Vector(1.0, 1.0, 1.0),
		Vector(1.0, 1.0, 1.0),
		Vector(1.0, 1.0, 1.0),
		Vector(1.0, 1.0, 1.0),
		Vector(1.0, 1.0, 1.0),
		Vector(1.0, 1.0, 1.0),
		Vector(1.0, 1.0, 1.0),
		Vector(1.0, 1.0, 1.0),
	};

	Color colors[] = {
		Color(0, 0, 1, 1),
		Color(1, 0, 1, 1),
		Color(1, 1, 1, 1),
		Color(0, 1, 1, 1),
		Color(1, 1, 1, 1),
		Color(1, 1, 0, 1),
		Color(1, 0, 0, 1),
		Color(1, 0, 1, 1),
		Color(0, 0, 0, 1),
	};

	TexCoord texcoords[] =
	{
		TexCoord(0.0, 0.0, 0.0),
		TexCoord(0.0, 1.0, 0.0),
		TexCoord(1.0, 1.0, 0.0),
		TexCoord(1.0, 0.0, 0.0),
		TexCoord(1.0, 0.0, 0.0),
		TexCoord(1.0, 1.0, 0.0),
		TexCoord(1.0, 1.0, 0.0),
		TexCoord(1.0, 0.0, 0.0),
		TexCoord(1.0, 0.0, 0.0),
	};

	int32_t vertices[] =
	{
		0,  1,  2,  3,  4,  5,
		6,  7,  8
	};

	// fill in the VBO buffers for this line:
	fillLineVBO(output, pointArr, normals, colors, texcoords, vertices, 9, 9, myNumVBOTexLayers);
	return;
}

void
ConvexHull::triangleGeometryVBO(SOP_VBOOutput* output)
{
	Vector normals[] =
	{
		Vector(1.0f, 0.0f, 0.0f), //v0
		Vector(0.0f, 1.0f, 0.0f), //v1
		Vector(0.0f, 0.0f, 1.0f), //v2
	};

	Color color[] =
	{
		Color(0.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
	};

	Position pointArr[] =
	{
		Position(0.0f, 0.0f, 0.0f),
		Position(0.0f, 1.0f, 0.0f),
		Position(1.0f, 0.0f, 0.0f),
	};

	int32_t vertices[] = { 0, 1, 2 };

	// fill in the VBO buffers for this triangle:
	fillFaceVBO(output, pointArr, normals, color, nullptr, vertices, 1, myNumVBOTexLayers, 1);

	return;
}

void
ConvexHull::particleGeometryVBO(SOP_VBOOutput* output)
{
	Position pointArr[] =
	{
		Position(-0.8f, 0.0f, 1.0f),
		Position(-0.6f, 0.4f, 1.0f),
		Position(-0.4f, 0.8f, 1.0f),
		Position(-0.2f, 0.4f, 1.0f),
		Position(0.0f,  0.0f, 1.0f),
		Position(0.2f, -0.4f, 1.0f),
		Position(0.4f, -0.8f, 1.0f),
		Position(0.6f, -0.4f, 1.0f),
		Position(0.8f, -0.2f, 1.0f),

		Position(-0.8f, 0.2f, 1.0f),
		Position(-0.6f, 0.6f, 1.0f),
		Position(-0.4f, 1.0f, 1.0f),
		Position(-0.2f, 0.6f, 1.0f),
		Position(0.0f,  0.2f, 1.0f),
		Position(0.2f, -0.2f, 1.0f),
		Position(0.4f, -0.6f, 1.0f),
		Position(0.6f, -0.2f, 1.0f),
		Position(0.8f,  0.0f, 1.0f),

		Position(-0.8f, -0.2f, 1.0f),
		Position(-0.6f,  0.2f, 1.0f),
		Position(-0.4f,  0.6f, 1.0f),
		Position(-0.2f,  0.2f, 1.0f),
		Position(0.0f, -0.2f, 1.0f),
		Position(0.2f, -0.6f, 1.0f),
		Position(0.4f, -1.0f, 1.0f),
		Position(0.6f, -0.6f, 1.0f),
		Position(0.8f, -0.4f, 1.0f),
	};


	Vector normals[] =
	{
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),

		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),

		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),
		Vector(1.0f, 1.0f, 1.0f),

	};

	Color colors[] =
	{
		Color(0.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(0.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 0.0f, 1.0f),
		Color(1.0f, 0.0f, 0.0f, 1.0f),
		Color(1.0f, 0.0f, 1.0f, 1.0f),
		Color(0.0f, 0.0f, 0.0f, 1.0f),

		Color(0.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(0.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 0.0f, 1.0f),
		Color(1.0f, 0.0f, 0.0f, 1.0f),
		Color(1.0f, 0.0f, 1.0f, 1.0f),
		Color(0.0f, 0.0f, 0.0f, 1.0f),

		Color(0.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 0.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(0.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 1.0f, 1.0f),
		Color(1.0f, 1.0f, 0.0f, 1.0f),
		Color(1.0f, 0.0f, 0.0f, 1.0f),
		Color(1.0f, 0.0f, 1.0f, 1.0f),
		Color(0.0f, 0.0f, 0.0f, 1.0f),
	};

	TexCoord texcoords[] =
	{
		TexCoord(0.0f, 0.0f, 0.0f),
		TexCoord(0.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),

		TexCoord(0.0f, 0.0f, 0.0f),
		TexCoord(0.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),

		TexCoord(0.0f, 0.0f, 0.0f),
		TexCoord(0.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 1.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
		TexCoord(1.0f, 0.0f, 0.0f),
	};

	int32_t vertices[] =
	{
		0,  1,  2,  3,  4,  5,
		6,  7,  8, 9, 10, 11, 12,
		13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26
	};

	// fill in the VBO buffers for this particle system:
	fillParticleVBO(output, pointArr, normals, colors, texcoords, vertices, 27, 27, myNumVBOTexLayers);
	return;
}


void
ConvexHull::executeVBO(SOP_VBOOutput* output,
						const OP_Inputs* inputs,
						void* reserved)
{
	myExecuteCount++;

	if (!output)
	{
		return;
	}

	if (inputs->getNumInputs() > 0)
	{
		// if there is a input node SOP node

		inputs->enablePar("Reset", 0);	// not used
		inputs->enablePar("Shape", 0);	// not used
		inputs->enablePar("Scale", 0);  // not used
	}
	else
	{
		inputs->enablePar("Shape", 0);

		inputs->enablePar("Scale", 1); // enable the scale selection
		double	 scale = inputs->getParDouble("Scale");

		// In this sample code an input CHOP node parameter is supported,
		// however it is possible to have DAT or TOP inputs as well

		const OP_CHOPInput	*cinput = inputs->getParCHOP("Chop");
		if (cinput)
		{
			int numSamples = cinput->numSamples;
			int ind = 0;
			myChopChanName = std::string(cinput->getChannelName(0));
			myChop = inputs->getParString("Chop");

			myChopChanVal = float(cinput->getChannelData(0)[ind]);
			scale = float(cinput->getChannelData(0)[ind] * scale);

		}

		// if the geometry have normals or colors, call enable functions:

		output->enableNormal();
		output->enableColor();
		// numLayers 1 means the texcoord will have 1 layer of uvw per each vertex:
		myNumVBOTexLayers = 1;
		output->enableTexCoord(myNumVBOTexLayers);

		// add custom attributes and access them in the GLSL (shader) code:
		SOP_CustomAttribInfo cu1("customColor", 4, AttribType::Float);
		output->addCustomAttribute(cu1);
		SOP_CustomAttribInfo cu2("customVert", 1, AttribType::Float);
		output->addCustomAttribute(cu2);

		// the number of vertices and index buffers must be set before generating any geometries:
		// set the bounding box for correct homing (specially for Straight to GPU mode):
#define CUBE_VBO 1
#define LINE_VBO 0
#define PARTICLE_VBO 0
#if CUBE_VBO
		{
			//draw Cube:
			int32_t numVertices = 36;
			int32_t numIndices = 36;

			output->allocVBO(numVertices, numIndices, VBOBufferMode::Static);

			cubeGeometryVBO(output, (float)scale);
			output->setBoundingBox(BoundingBox(1.0f, -1.0f, -1.0f, 3.0f, 1.0f, 1.0f));
		}
#elif LINE_VBO
		{
			// draw Line:
			int32_t numVertices = 10;
			int32_t numIndices = 10;

			output->allocVBO(numVertices, numIndices, VBOBufferMode::Static);

			lineGeometryVBO(output);
		}
#elif PARTICLE_VBO
		{
			// draw Particle System:
			int32_t numVertices = 27;
			int32_t numIndices = 27;

			output->allocVBO(numVertices, numIndices, VBOBufferMode::Static);

			particleGeometryVBO(output);
		}
#endif

		// once the geometry VBO buffers are filled in, call this function as the last function
		output->updateComplete();

	}

}

//-----------------------------------------------------------------------------------------------------
//								CHOP, DAT, and custom parameters
//-----------------------------------------------------------------------------------------------------

int32_t
ConvexHull::getNumInfoCHOPChans(void* reserved)
{
	// We return the number of channel we want to output to any Info CHOP
	// connected to the CHOP. In this example we are just going to send 4 channels.
	return 4;
}

void
ConvexHull::getInfoCHOPChan(int32_t index,
								OP_InfoCHOPChan* chan, void* reserved)
{
	// This function will be called once for each channel we said we'd want to return
	// In this example it'll only be called once.

	if (index == 0)
	{
		chan->name->setString("executeCount");
		chan->value = (float)myExecuteCount;
	}

	if (index == 1)
	{
		chan->name->setString("offset");
		chan->value = (float)myOffset;
	}

	if (index == 2)
	{
		chan->name->setString(myChop.c_str());
		chan->value = (float)myOffset;
	}

	if (index == 3)
	{
		chan->name->setString(myChopChanName.c_str());
		chan->value = myChopChanVal;
	}
}

bool
ConvexHull::getInfoDATSize(OP_InfoDATSize* infoSize, void* reserved)
{
	infoSize->rows = 3;
	infoSize->cols = 2;
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
	char tempBuffer[4096];

	if (index == 0)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "executeCount");
#else // macOS
		strlcpy(tempBuffer, "executeCount", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%d", myExecuteCount);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%d", myExecuteCount);
#endif
		entries->values[1]->setString(tempBuffer);
	}

	if (index == 1)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "offset");
#else // macOS
		strlcpy(tempBuffer, "offset", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		sprintf_s(tempBuffer, "%g", myOffset);
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%g", myOffset);
#endif
		entries->values[1]->setString(tempBuffer);
	}

	if (index == 2)
	{
		// Set the value for the first column
#ifdef _WIN32
		strcpy_s(tempBuffer, "DAT input name");
#else // macOS
		strlcpy(tempBuffer, "offset", sizeof(tempBuffer));
#endif
		entries->values[0]->setString(tempBuffer);

		// Set the value for the second column
#ifdef _WIN32
		strcpy_s(tempBuffer, myDat.c_str());
#else // macOS
		snprintf(tempBuffer, sizeof(tempBuffer), "%g", myOffset);
#endif
		entries->values[1]->setString(tempBuffer);
	}
}



void
ConvexHull::setupParameters(OP_ParameterManager* manager, void* reserved)
{
	// CHOP
	{
		OP_StringParameter	np;

		np.name = "Chop";
		np.label = "CHOP";

		OP_ParAppendResult res = manager->appendCHOP(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// scale
	{
		OP_NumericParameter	np;

		np.name = "Scale";
		np.label = "Scale";
		np.defaultValues[0] = 1.0;
		np.minSliders[0] = -10.0;
		np.maxSliders[0] = 10.0;

		OP_ParAppendResult res = manager->appendFloat(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// shape
	{
		OP_StringParameter	sp;

		sp.name = "Shape";
		sp.label = "Shape";

		sp.defaultValue = "Cube";

		const char *names[] = { "Cube", "Triangle", "Line" };
		const char *labels[] = { "Cube", "Triangle", "Line" };

		OP_ParAppendResult res = manager->appendMenu(sp, 3, names, labels);
		assert(res == OP_ParAppendResult::Success);
	}

	// GPU Direct
	{
		OP_NumericParameter np;

		np.name = "Gpudirect";
		np.label = "GPU Direct";

		OP_ParAppendResult res = manager->appendToggle(np);
		assert(res == OP_ParAppendResult::Success);
	}

	// pulse
	{
		OP_NumericParameter	np;

		np.name = "Reset";
		np.label = "Reset";

		OP_ParAppendResult res = manager->appendPulse(np);
		assert(res == OP_ParAppendResult::Success);
	}

}

void
ConvexHull::pulsePressed(const char* name, void* reserved)
{
	if (!strcmp(name, "Reset"))
	{
		myOffset = 0.0;
	}
}

