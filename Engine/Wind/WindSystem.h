#pragma once

#include "Engine/ECS/System.h"

// Grid dimensions include simulation and 1 block around boundry
#define WIND_SIM_GRID_DIMENSIONS 50
// Each tile of the wind simulation represents 1 meter of real world space
#define WIND_SIM_TILE_SIZE 1
// Non square shaped simulation grids not yet implemented, so X and Y must be the same
#define WIND_SIM_GRID_DIMENSIONS_X WIND_SIM_GRID_DIMENSIONS
#define WIND_SIM_GRID_DIMENSIONS_Y WIND_SIM_GRID_DIMENSIONS

// Forward Declarations
namespace Mega
{
	class Engine;
}

namespace Mega
{
	// Handles all wind throughout the engine (fluid sim, wind motors, wind recievers...)
	class WindSystem : public System
	{
	public:
		friend Engine;

		using tScalar = float; // Wind simulation precision
		using Vec2 = glm::vec<2, tScalar, glm::packed_highp>;
		using tWindVector = glm::vec<4, tScalar, glm::packed_highp>;

		eMegaResult OnInitialize() override;
		eMegaResult OnUpdate(const tTimestep in_dt, Scene* in_pScene) override;
		eMegaResult OnDestroy() override;

		// Getters / Setters
		inline const std::vector<Vec4>& GetGlobalWindData() const { return m_globalWindData; }
		inline const Vec4 GetGlobalWindVector() const { return Vec4(normalize(Vec3(m_globalWindVector)), m_globalWindVector.w); }
		inline Vec3 GetWindSimulationCenter() const { return m_windSimCenter; }
		inline void SetWindSimulationCenter(const Vec3& in_center) { m_windSimCenter = in_center; }

		struct FluidSimulator2D
		{
		public:
			friend WindSystem;
			using tDataArray = std::vector<tScalar>;

			eMegaResult Initialize();
			eMegaResult Update(const tTimestep in_dt);
			eMegaResult Destroy();

			void AddDensity(const Vec2& in_pos, const tScalar in_density);
			void AddVelocity(const Vec2& in_pos, const Vec2& in_velocity);
			void ClearVelocity();

			// Fills a vector with fluid data (changes array structure to AoS (array of vecs vs 4 arrays for each x, y, z, a) because that is how it is most likely
			// going to be used especially by the gpu)
			void FillVelocityData(std::vector<Vec4>& in_buffer);

			// Number of blocks in the 3d grid
			inline constexpr uint64_t GetBlockCount() const { return (uint64_t)(WIND_SIM_GRID_DIMENSIONS_X * WIND_SIM_GRID_DIMENSIONS_Y); }

		private:
			// ---------------- Private Fluid Simulation Steps/Helpers ------------------- //
			void Diffuse(uint32_t in_callIndex, tDataArray& in_pX, tDataArray& in_pX0, const tScalar in_value, const tTimestep in_dt);
			void SolveLinearEquation(uint32_t in_callIndex, tDataArray& in_pX, tDataArray& in_pX0, const tScalar in_a, const tScalar in_c);
			void Project(tDataArray& in_pVelocityX, tDataArray& in_pVelocityY, tDataArray& in_pP, tDataArray& in_pDiv);
			void Advect(uint32_t in_callIndex, tDataArray& in_pD, tDataArray& in_pD0, tDataArray& in_pVelocityX, tDataArray& in_pVelocityY, const tTimestep in_dt);
			void SetBoundry(const int32_t b, tDataArray& x);

			const Vec2U m_gridDimensions = Vec2U(WIND_SIM_GRID_DIMENSIONS_X, WIND_SIM_GRID_DIMENSIONS_Y);

			uint32_t m_diffusionPrecision = 20;
			tScalar m_diffusionRate = 0.0f;
			tScalar m_viscosity = 0.02;

			// Arrays of grid data, they are 3 dimensional but stored as 1D arrays for effeciency
			tDataArray m_pTemp{};
			tDataArray m_pDensityData{};

			// SoA type array architecture to be more cache friendly
			tDataArray m_pVelocityDataX{};
			tDataArray m_pVelocityDataY{};
			tDataArray m_pVelocityDataX0{};
			tDataArray m_pVelocityDataY0{};
		};

	private:
		Vec3 m_windSimCenter = { 0, 0, 0 };
		tWindVector m_globalWindVector = { 1, 0, 0, 1 }; // xyz dir, w magnitude
		std::vector<tWindVector> m_globalWindData{};

		FluidSimulator2D m_windSimulator{};
	};
}