#include "WindSystem.h"

#include <algorithm>

#include "ImGui/imgui.h"
#include "Engine/Core/Time.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Wind/WindComponents.h"
#include "Engine/Engine.h"

#define IX(x, y) ((x) + (y) * WIND_SIM_GRID_DIMENSIONS_Y) // Used to access a 1D vector of data as a 2D / 3D matrix
glm::vec2 g_dir = { 0, 1 };
float g_str = 50.0f;
namespace Mega
{
	// ================ Wind System ================ //
	eMegaResult WindSystem::OnInitialize()
	{
		m_windSimulator.Initialize();

		m_globalWindData.resize(m_windSimulator.GetBlockCount());

		return eMegaResult::SUCCESS;
	};

	bool g_b = false;
	float g_dt = 1000;
	eMegaResult WindSystem::OnUpdate(const tTimestep in_dt, Scene* in_pScene)
	{
		///////////////////// AHHHHHH ///////////////////////////////


		ImGui::DragFloat4("Global Wind Vector", &m_globalWindVector.x, 0.01f);


		/////////////////////////////////////////////////////////////
		
		// Update wind simulation using the new data
		m_windSimulator.Update(in_dt);
		m_windSimulator.FillVelocityData(m_globalWindData);

		// Send wind motor component data to wind simulation
		const auto& viewMotors = in_pScene->GetRegistry().view<Component::WindMotor>();
		for (const auto& [entity, motor] : viewMotors.each())
		{
			// Each wind motor component adds wind to the simulation at a specific position
			if (motor.isOn)
			{
				m_windSimulator.AddVelocity(Vec2(motor.position.x, motor.position.z), Vec2(motor.forceVector.x, motor.forceVector.z));
				motor.isOn = false;
			}
		}


		ImGui::DragFloat("Wind Strength", &g_str, 0.001, 0, 50);
		ImGui::Checkbox("Add Wind", &g_b);
		if (g_b)
		{
			//m_windSimulator.AddVelocity(Vec2(5, 5), normalize(Vec2(cos(Engine::Runtime() / 1000.0) + 1, sin(Engine::Runtime() / 1000.0) + 1)) * Vec2(15));
			m_windSimulator.AddVelocity(Vec2(9, 9), Vec2(1, 1) * g_str);
			//m_windSimulator.AddVelocity(Vec2(10, 9), Vec2(-1, 1) * g_str);
			//m_windSimulator.AddVelocity(Vec2(9, 10), Vec2(1, -1) * g_str);
			//m_windSimulator.AddVelocity(Vec2(10, 10), Vec2(-1, -1) * g_str);
		}

		// ------------------- ImGui Display ----------------------- //

		// TODO: Confirm simulation working using densisites (display that in buttons instead of velocity)
		ImGui::NewLine();
		for (uint32_t m = 0; m < m_windSimulator.m_gridDimensions.x; m++)
		{
			for (uint32_t i = 0; i < m_windSimulator.m_gridDimensions.y; i++)
			{
				Vec4 windData = m_globalWindData[IX(i, m)];
				if (ImGui::ColorButton(std::string(std::to_string(m + i)).c_str(), ImVec4(windData.x, \
					windData.z, 0, 1)))
				{
					m_windSimulator.AddDensity(Vec2(i, m), 1);
				}
				ImGui::SameLine();
			}
			ImGui::NewLine();
		}
		// --------------------------------------------------------- //

		// Read back wind data from the simulation and send it to the reciever components
		const auto& viewRecievers = in_pScene->GetRegistry().view<Component::WindReciever>();
		for (const auto& [entity, reciever] : viewRecievers.each())
		{
			//reciever.AddWindData(m_windSimulator.GetWindData(reciever.position));
			MEGA_ASSERT(false, "No wind reciever code yet");
		}

		return eMegaResult::SUCCESS;
	};

	eMegaResult WindSystem::OnDestroy()
	{
		m_windSimulator.Destroy();

		return eMegaResult::SUCCESS;
	}
}
	
	// =========== Fluid Simulation ============ //
namespace Mega
{
	eMegaResult WindSystem::FluidSimulator2D::Initialize()
	{
		const uint64_t gridBlockCount = GetBlockCount();

		m_pTemp.resize(gridBlockCount);
		m_pDensityData.resize(gridBlockCount);

		m_pVelocityDataX.resize(gridBlockCount);
		m_pVelocityDataY.resize(gridBlockCount);

		m_pVelocityDataX0.resize(gridBlockCount);
		m_pVelocityDataY0.resize(gridBlockCount);

		return eMegaResult::SUCCESS;
	}

	eMegaResult WindSystem::FluidSimulator2D::Update(const tTimestep in_dt)
	{
		ImGui::DragFloat("Scale Wind DT", &g_dt, 1, 1);
		ImGui::DragFloat("Wind Viscosity", &m_viscosity, 0.0001, 0.000001);
		int i = m_diffusionPrecision;
		ImGui::DragInt("Wind DP", &i, 1, 1, 20);
		m_diffusionPrecision = i;
		const tTimestep scaled_dt = in_dt / g_dt;

		// ----------------------- Fluid Simulation Step ----------------------- //
		Diffuse(1, m_pVelocityDataX0, m_pVelocityDataX, m_viscosity, scaled_dt);
		Diffuse(2, m_pVelocityDataY0, m_pVelocityDataY, m_viscosity, scaled_dt);

		Project(m_pVelocityDataX0, m_pVelocityDataY0, m_pVelocityDataX, m_pVelocityDataY);
		
		Advect(1, m_pVelocityDataX, m_pVelocityDataX0, m_pVelocityDataX0, m_pVelocityDataY0, scaled_dt);
		Advect(2, m_pVelocityDataY, m_pVelocityDataY0, m_pVelocityDataX0, m_pVelocityDataY0, scaled_dt);
		
		Project(m_pVelocityDataX, m_pVelocityDataY, m_pVelocityDataX0, m_pVelocityDataY0);

		Diffuse(0, m_pTemp, m_pDensityData, m_diffusionRate, scaled_dt);
		Advect(0, m_pDensityData, m_pTemp, m_pVelocityDataX, m_pVelocityDataY, scaled_dt);

		return eMegaResult::SUCCESS;
	}

	eMegaResult WindSystem::FluidSimulator2D::Destroy()
	{
		return eMegaResult::SUCCESS;
	}

	void WindSystem::FluidSimulator2D::FillVelocityData(std::vector<Vec4>& in_buffer)
	{
		const size_t expectedBufferSize = GetBlockCount() * sizeof(Vec4);
		const size_t inputBufferSize    = in_buffer.size() * sizeof(in_buffer[0]);
		MEGA_ASSERT(inputBufferSize == expectedBufferSize, "Buffer sizes not equal");

		for (uint32_t x = 0; x < WIND_SIM_GRID_DIMENSIONS_X; x++)
		{
			for (uint32_t y = 0; y < WIND_SIM_GRID_DIMENSIONS_Y; y++)
			{
				// TODO: optimiza
				const size_t index = IX(x, y);

				Vec4 vec{};

				vec.x = (m_pVelocityDataX[index]) / 5.0f;
				vec.y = (m_pVelocityDataY[index]);
				vec.z = (m_pVelocityDataY[index]) / 5.0f;
				vec.w = std::min<tScalar>(1, glm::length(Vec3(vec)));
				
				if (vec.w > 0)
				{
					Vec3 norm = Vec3(vec);
					vec.x = norm.x;
					vec.y = norm.y;
					vec.z = norm.z;
				}

				in_buffer[IX(x, y)] = vec;
			}
		}
	}

	void WindSystem::FluidSimulator2D::ClearVelocity()
	{
		for (uint32_t x = 0; x < WIND_SIM_GRID_DIMENSIONS_X; x++)
		{
			for (uint32_t y = 0; y < WIND_SIM_GRID_DIMENSIONS_Y; y++)
			{
				const size_t index = IX(x, y);

				m_pVelocityDataX[index]  = 0;
				m_pVelocityDataX0[index] = 0;
				m_pVelocityDataY[index]  = 0;
				m_pVelocityDataY0[index] = 0;
			}
		}
	}

	// Diffusion step: the act of the dnesitiy spreading across to all of its neighboring grid blocks
	void WindSystem::FluidSimulator2D::Diffuse(uint32_t in_callIndex, tDataArray& in_pX, tDataArray& in_pX0, const tScalar in_value, const tTimestep in_dt)
	{
		const tScalar a = in_dt * in_value * (WIND_SIM_GRID_DIMENSIONS - 2) * (WIND_SIM_GRID_DIMENSIONS - 2); // Minus 2 because one block on each side of the grid is reserved for boundries
		const tScalar c = 1 + 4 * a; // + 6 because thats how many blocks are check in the linear equation step (above, below, left, right, front, back)

		SolveLinearEquation(in_callIndex, in_pX, in_pX0, a, c); // TODO: dont need in_dims
	}

	void WindSystem::FluidSimulator2D::SolveLinearEquation(uint32_t in_callIndex, tDataArray& in_pX, tDataArray& in_pX0, const tScalar in_a, const tScalar in_c)
	{
		const tScalar cRecipricol = 1.0f / in_c;
		// Precision of how solver or number of times we solve the linear equation
		for (uint32_t i = 0; i < m_diffusionPrecision; i++)
		{
			// Equivalent to i = 1, i <= N (N here is the size of grid dimensions without the border)
			for (uint32_t i = 1; i < WIND_SIM_GRID_DIMENSIONS_X - 1; i++)
			{
				for (int32_t j = 1; j < WIND_SIM_GRID_DIMENSIONS_Y - 1; j++)
				{
					in_pX[IX(i, j)] = (in_pX0[IX(i, j)] + in_a * (in_pX[IX(i - 1, j)] + in_pX[IX(i + 1, j)] +
						                                          in_pX[IX(i, j - 1)] + in_pX[IX(i, j + 1)])) * cRecipricol;
				}
			}
		}

		SetBoundry(in_callIndex, in_pX);
	}

	// Used to conserve a constant mass throughout the grid (water is incompressible so the amount of water in each block
	// should stay the same throughout the simulation)
	// "Hodge decomposition: every velocity field is the sum of a mass conserving fieldand a gradient field"
	// "Imagine a terrain with hillsand valleys with an arrow at every point pointing in the direction of steepest
	//	descent. Computing the gradient is then equivalent to computing a height field. Once we have
	//	this height field we can subtract its gradient from our velocity field to get a mass conserving one"
	void WindSystem::FluidSimulator2D::Project(tDataArray& in_pVelocityX, tDataArray& in_pVelocityY, tDataArray& in_pP, tDataArray& in_pDiv)
	{
		// Triple for loop to traverse the whole 3d grid (orederd z, y, x)
		const tScalar h = 1.0f / (WIND_SIM_GRID_DIMENSIONS - 2);

		for (uint32_t i = 1; i < m_gridDimensions.x - 1; i++)
		{
			for (uint32_t j = 1; j < m_gridDimensions.y - 1; j++)
			{
				in_pDiv[IX(i, j)] = -0.5f * h * (
					  in_pVelocityX[IX(i + 1, j)]
					- in_pVelocityX[IX(i - 1, j)]
					+ in_pVelocityY[IX(i, j + 1)]
					- in_pVelocityY[IX(i, j - 1)]
					);

				in_pP[IX(i, j)] = 0;
			}
		}

		SetBoundry(0, in_pDiv);
		SetBoundry(0, in_pP);

		SolveLinearEquation(0, in_pP, in_pDiv, 1, 6);

		for (uint32_t i = 1; i < m_gridDimensions.x - 1; i++)
		{
			for (uint32_t j = 1; j < m_gridDimensions.y - 1; j++)
			{
				in_pVelocityX[IX(i, j)] -= 0.5f * (in_pP[IX(i + 1, j)] - in_pP[IX(i - 1, j)]) / h;
				in_pVelocityX[IX(i, j)] -= 0.5f * (in_pP[IX(i, j + 1)] - in_pP[IX(i, j - 1)]) / h;
			}
		}

		SetBoundry(1, in_pVelocityX);
		SetBoundry(2, in_pVelocityY);
	}

	void WindSystem::FluidSimulator2D::Advect(uint32_t in_callIndex, tDataArray& in_pD, tDataArray& in_pD0, tDataArray& in_pVelocityX, tDataArray& in_pVelocityY, const tTimestep in_dt)
	{
		const tScalar dt0 = (tScalar)in_dt * (WIND_SIM_GRID_DIMENSIONS - 2);

		for (uint32_t i = 1; i < m_gridDimensions.x - 1; i++)
		{
			for (uint32_t j = 1; j < m_gridDimensions.y - 1; j++)
			{
				tScalar x = i - dt0 * in_pVelocityX[IX(i, j)];
				tScalar y = j - dt0 * in_pVelocityY[IX(i, j)];

				x = std::max(x, (tScalar)0.5);
				y = std::max(y, (tScalar)0.5);

				if (x > WIND_SIM_GRID_DIMENSIONS + 0.5) { x = WIND_SIM_GRID_DIMENSIONS + 0.5; }
				if (y > WIND_SIM_GRID_DIMENSIONS + 0.5) { y = WIND_SIM_GRID_DIMENSIONS + 0.5; }

				const int32_t i0 = (int)x;
				const int32_t i1 = i0 + 1;
				const int32_t j0 = (int)y;
				const int32_t j1 = j0 + 1;

				const tScalar s1 = x - i0;
				const tScalar s0 = 1 - s1;
				const tScalar t1 = y - j0;
				const tScalar t0 = 1 - t1;

				in_pD[IX(i, j)] = s0 * (t0 * in_pD0[IX(i0, j0)] + t1 * in_pD0[IX(i0, j1)]) +
					              s1 * (t0 * in_pD0[IX(i1, j0)] + t1 * in_pD0[IX(i1, j1)]);
			}
		}

		SetBoundry(in_callIndex, in_pD);
	}

	void WindSystem::FluidSimulator2D::SetBoundry(const int32_t b, tDataArray& x)
	{
		// N here is the size of grid dimensions without the border, so - 2
		const int32_t N = WIND_SIM_GRID_DIMENSIONS - 2;

		for (int32_t i = 1; i < WIND_SIM_GRID_DIMENSIONS - 1; i++)
		{
			x[IX(0, i)]     = b == 1 ? -x[IX(1, i)] : x[IX(1, i)];
			x[IX(N + 1, i)] = b == 1 ? -x[IX(N, i)] : x[IX(N, i)];
			x[IX(i, 0)]     = b == 2 ? -x[IX(i, 1)] : x[IX(i, 1)];
			x[IX(i, N + 1)] = b == 2 ? -x[IX(i, N)] : x[IX(i, N)];
		}

		x[IX(0, 0)]         = 0.5f * (x[IX(1, 0)]     + x[IX(0, 1)]);
		x[IX(0, N + 1)]     = 0.5f * (x[IX(1, N + 1)] + x[IX(0, N)]);
		x[IX(N + 1, 0)]     = 0.5f * (x[IX(N, 0)]     + x[IX(N + 1, 1)]);
		x[IX(N + 1, N + 1)] = 0.5f * (x[IX(N, N + 1)] + x[IX(N + 1, N)]);

	}

	void WindSystem::FluidSimulator2D::AddDensity(const Vec2& in_pos, const tScalar in_density)
	{
		int index = IX((int)in_pos.x, (int)in_pos.y);
		m_pDensityData[index] += in_density;
	}

	void WindSystem::FluidSimulator2D::AddVelocity(const Vec2& in_pos, const Vec2& in_velocity)
	{
		// TODO
		if (in_pos.x < 0 || in_pos.x >= WIND_SIM_GRID_DIMENSIONS_X) { return; }
		if (in_pos.y < 0 || in_pos.y >= WIND_SIM_GRID_DIMENSIONS_Y) { return; }

		const int index = IX((int)in_pos.x, (int)in_pos.y);

		m_pVelocityDataX[index] += in_velocity.x;
		m_pVelocityDataY[index] += in_velocity.y;
	}
} // namespace Mega

#undef IX