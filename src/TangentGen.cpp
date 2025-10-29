#include "TangentGen.hpp"
#include <stdexcept>

// Define the static SMikkTSpaceInterface once
SMikkTSpaceInterface& TangentGenerator::GetInterface()
{
	static SMikkTSpaceInterface iface{};

	if (iface.m_getNumFaces == nullptr)
	{
		iface.m_getNumFaces = TangentGenerator::get_num_faces;
		iface.m_getNumVerticesOfFace = TangentGenerator::get_num_vertices_of_face;
		iface.m_getNormal = TangentGenerator::get_normal;
		iface.m_getPosition = TangentGenerator::get_position;
		iface.m_getTexCoord = TangentGenerator::get_tex_coords;
		iface.m_setTSpaceBasic = TangentGenerator::set_tspace_basic;
	}
	return iface;
}

void TangentGenerator::CalculateTangents(MikkTSpaceData& data)
{
	// Ensure we are only working with triangles so our index buffer assumption holds
	if (data.indexCount % 3 != 0)
	{
		throw std::runtime_error("TangentGenerator: indexCount is not a multiple of 3 - mesh must be triangulated");
	}

	if (data.indexOffset + data.indexCount > data.allIndicesPtr->size()) {
		throw std::runtime_error("TangentGenerator: index range exceeds available indices.");
	}

	SMikkTSpaceContext context{};
	context.m_pInterface = &GetInterface();
	context.m_pUserData = &data;

	if (!genTangSpaceDefault(&context))
	{
		throw std::runtime_error("MikkTSpace failed to generate tangents!");
	}
}

// Helper to get the index into the global allVertices array
uint32_t TangentGenerator::get_global_vertex_index(const SMikkTSpaceContext* context, int iface, int iVert)
{
	MikkTSpaceData* data = static_cast<MikkTSpaceData*>(context->m_pUserData);
	uint32_t relative_index_in_indices = data->indexOffset + (iface * 3) + iVert;

	// The index stored in allIndices is relative to the mesh's vertexOffset
	uint32_t mesh_local_index = (*data->allIndicesPtr)[relative_index_in_indices];

	// The global index in allVertices
	return data->vertexOffset + mesh_local_index;
}

// 1. Get total number of faces (triangles)
int TangentGenerator::get_num_faces(const SMikkTSpaceContext* context)
{
	MikkTSpaceData* data = static_cast<MikkTSpaceData*>(context->m_pUserData);
	return data->indexCount / 3;
}

// 2. Get number of vertices per face (always 3 for triangles)
int TangentGenerator::get_num_vertices_of_face(const SMikkTSpaceContext* context, int iFace)
{
	return 3;
}

// 3. Get vertex position
void TangentGenerator::get_position(const SMikkTSpaceContext* context, float outpos[], int iFace, int iVert)
{
	MikkTSpaceData* data = static_cast<MikkTSpaceData*>(context->m_pUserData);
	uint32_t global_index = get_global_vertex_index(context, iFace, iVert);
	const Vertex& vertex = (*data->allVerticesPtr)[global_index];
	outpos[0] = vertex.pos.x;
	outpos[1] = vertex.pos.y;
	outpos[2] = vertex.pos.z;
}

// 4. Get vertex normal
void TangentGenerator::get_normal(const SMikkTSpaceContext* context, float outnormal[], int iFace, int iVert)
{
	MikkTSpaceData* data = static_cast<MikkTSpaceData*>(context->m_pUserData);
	uint32_t global_index = get_global_vertex_index(context, iFace, iVert);
	const Vertex& vertex = (*data->allVerticesPtr)[global_index];
	outnormal[0] = vertex.normal.x;
	outnormal[1] = vertex.normal.y;
	outnormal[2] = vertex.normal.z;
}

void TangentGenerator::get_tex_coords(const SMikkTSpaceContext* context, float outuv[], int iFace, int iVert)
{
	MikkTSpaceData* data = static_cast<MikkTSpaceData*>(context->m_pUserData);
	uint32_t global_index = get_global_vertex_index(context, iFace, iVert);
	const Vertex& vertex = (*data->allVerticesPtr)[global_index];
	outuv[0] = vertex.texCoord.x;
	outuv[1] = vertex.texCoord.y;
}

void TangentGenerator::set_tspace_basic(const SMikkTSpaceContext* context, const float tangentu[], float fSign, int iFace, int iVert)
{
	MikkTSpaceData* data = static_cast<MikkTSpaceData*>(context->m_pUserData);
	uint32_t global_index = get_global_vertex_index(context, iFace, iVert);
	Vertex& vertex = (*data->allVerticesPtr)[global_index];

	// Note: glm::vec4 tangent is already in your vertex struct
	vertex.tangent = glm::vec4(tangentu[0], tangentu[1], tangentu[2], fSign);
}