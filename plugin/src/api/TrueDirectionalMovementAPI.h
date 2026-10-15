#pragma once
#include <functional>
#include <stdint.h>

// True Directional Movement's modder interface, copied from its author's repository as the file itself asks
// (github.com/ersh1/TrueDirectionalMovement, src/TrueDirectionalMovementAPI.h; GPL-3.0, as this mod).
// Only change: GetModuleHandleA, since this project builds with UNICODE.

/*
* For modders: Copy this file into your own project if you wish to use this API
*/
namespace TDM_API
{
	constexpr const auto TDMPluginName = "TrueDirectionalMovement";

	using PluginHandle = SKSE::PluginHandle;
	using ActorHandle = RE::ActorHandle;

	// Available True Directional Movement interface versions
	enum class InterfaceVersion : uint8_t
	{
		V1,
		V2,
		V3,
		V4,
		V5
	};

	// Error types that may be returned by the True Directional Movement API
	enum class APIResult : uint8_t
	{
		// Your API call was successful
		OK,

		// You tried to release a resource that was not allocated to you
		// Do not attempt to manipulate the requested resource if you receive this response
		NotOwner,

		// True Directional Movement currently must keep control of this resource for proper functionality
		// Do not attempt to manipulate the requested resource if you receive this response
		MustKeep,

		// You have already been given control of this resource
		AlreadyGiven,

		// Another mod has been given control of this resource at the present time
		// Do not attempt to manipulate the requested resource if you receive this response
		AlreadyTaken,

		// You sent a command on a thread that could cause a data race were it to be processed
		// Do not attempt to manipulate the requested resource if you receive this response
		BadThread,
	};

	enum class DirectionalMovementMode : uint8_t
	{
		// Directional movement is disabled, the movement is vanilla
		kDisabled,

		// Directional movement is enabled, but the player is facing the crosshair
		kVanillaStyle,

		// Directional movement is enabled, and the player is facing the direction of movement
		kDirectional,

		// Directional movement is enabled, and the player has a target locked
		kTargetLock
	};

	// True Directional Movement's modder interface
	class IVTDM1
	{
	public:
		[[nodiscard]] virtual unsigned long GetTDMThreadId() const noexcept = 0;
		[[nodiscard]] virtual bool GetDirectionalMovementState() const noexcept = 0;
		[[nodiscard]] virtual bool GetTargetLockState() const noexcept = 0;
		[[nodiscard]] virtual ActorHandle GetCurrentTarget() const noexcept = 0;
		[[nodiscard]] virtual APIResult RequestDisableDirectionalMovement(PluginHandle a_myPluginHandle) noexcept = 0;
		[[nodiscard]] virtual APIResult RequestDisableHeadtracking(PluginHandle a_myPluginHandle) noexcept = 0;
		virtual PluginHandle GetDisableDirectionalMovementOwner() const noexcept = 0;
		virtual PluginHandle GetDisableHeadtrackingOwner() const noexcept = 0;
		virtual APIResult ReleaseDisableDirectionalMovement(PluginHandle a_myPluginHandle) noexcept = 0;
		virtual APIResult ReleaseDisableHeadtracking(PluginHandle a_myPluginHandle) noexcept = 0;
	};

	class IVTDM2 : public IVTDM1
	{
	public:
		[[nodiscard]] virtual APIResult RequestYawControl(PluginHandle a_myPluginHandle, float a_yawRotationSpeedMultiplier) noexcept = 0;
		virtual APIResult SetPlayerYaw(PluginHandle a_myPluginHandle, float a_desiredYaw) noexcept = 0;
		virtual APIResult ReleaseYawControl(PluginHandle a_myPluginHandle) noexcept = 0;
	};

	class IVTDM3 : public IVTDM2
	{
	public:
		[[nodiscard]] virtual DirectionalMovementMode GetDirectionalMovementMode() const noexcept = 0;
		[[nodiscard]] virtual RE::NiPoint2 GetActualMovementInput() const noexcept = 0;
	};

	class IVTDM4 : public IVTDM3
	{
	public:
		[[nodiscard]] virtual bool IsTargetLockBehindTarget() const noexcept = 0;
	};

	class IVTDM5 : public IVTDM4
	{
	public:
		[[nodiscard]] virtual APIResult RequestDisableTargetLock(PluginHandle a_myPluginHandle) noexcept = 0;
		virtual APIResult ReleaseDisableTargetLock(PluginHandle a_myPluginHandle) noexcept = 0;
		virtual PluginHandle GetDisableTargetLockOwner() const noexcept = 0;
	};

	typedef void* (*_RequestPluginAPI)(const InterfaceVersion interfaceVersion);

	/// Request the True Directional Movement API interface.
	/// Recommended: Send your request during or after SKSEMessagingInterface::kMessage_PostLoad to make sure the dll has already been loaded
	[[nodiscard]] inline void* RequestPluginAPI(const InterfaceVersion a_interfaceVersion = InterfaceVersion::V5)
	{
		auto              pluginHandle = GetModuleHandleA("TrueDirectionalMovement.dll");
		_RequestPluginAPI requestAPIFunction = pluginHandle ? (_RequestPluginAPI)GetProcAddress(pluginHandle, "RequestPluginAPI") : nullptr;
		if (requestAPIFunction) {
			return requestAPIFunction(a_interfaceVersion);
		}
		return nullptr;
	}
}
