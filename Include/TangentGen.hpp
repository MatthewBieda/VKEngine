#pragma once

#include "Vertex.hpp"
#include "mikktspace.h"

#include <vector>

struct MikkTSpaceData
{
	// Pointers to global data buffers
	std::vector<Vertex>* allVerticesPtr;
	const std::vector<uint32_t>* allIndicesPtr;
	
	// Offsets/Counts for the current mesh/submesh
	uint32_t vertexOffset;
	uint32_t indexOffset;
	uint32_t indexCount;
};

class TangentGenerator
{
public:
	// Call once to get the static interface used for MikkTSpace
	static SMikkTSpaceInterface& GetInterface();

	// Call to run the calculation for a submesh
	static void CalculateTangents(MikkTSpaceData& data);

private:
	// Static MikkTSpace callbacks

	// Helper to get the index into the global allVertices array
	static uint32_t get_global_vertex_index(const SMikkTSpaceContext* context, int iface, int iVert);

	// SMikkTIinterace required functions
	static int get_num_faces(const SMikkTSpaceContext* context);
	static int get_num_vertices_of_face(const SMikkTSpaceContext* context, int iFace);
	static void get_position(const SMikkTSpaceContext* context, float outpos[], int iFace, int iVert);
	static void get_normal(const SMikkTSpaceContext* context, float outnormal[], int iFace, int iVert);
	static void get_tex_coords(const SMikkTSpaceContext* context, float outuv[], int iFace, int iVert);
	static void set_tspace_basic(const SMikkTSpaceContext* context, const float tangentu[], float fSign, int iFace, int iVert);
};