#pragma once

namespace dm
{
	// Quaternion (x, y, z, w), Hamilton product, v' = q v q^-1. Matches the converted clip data.
	struct Quat
	{
		float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
	};

	inline Quat Mul(const Quat& a, const Quat& b)
	{
		return {
			a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
			a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
			a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
			a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
		};
	}

	inline Quat Normalized(const Quat& q)
	{
		const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
		return n > 1e-8f ? Quat{ q.x / n, q.y / n, q.z / n, q.w / n } : Quat{};
	}

	inline Quat Nlerp(const Quat& a, Quat b, float t)
	{
		if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0.0f) {
			b = { -b.x, -b.y, -b.z, -b.w };
		}
		return Normalized({ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t });
	}

	inline Quat Inverse(const Quat& q)
	{
		return { -q.x, -q.y, -q.z, q.w };
	}

	inline RE::NiPoint3 Rotate(const Quat& q, const RE::NiPoint3& v)
	{
		const RE::NiPoint3 u{ q.x, q.y, q.z };
		const RE::NiPoint3 t = u.Cross(v) * 2.0f;
		return v + t * q.w + u.Cross(t);
	}

	// The part of q that is a rotation about the unit axis (swing-twist decomposition), by the shorter way round.
	inline Quat TwistAbout(const Quat& q, const RE::NiPoint3& a_axis)
	{
		const float d = q.x * a_axis.x + q.y * a_axis.y + q.z * a_axis.z;
		Quat        twist = Normalized({ a_axis.x * d, a_axis.y * d, a_axis.z * d, q.w });
		if (twist.w < 0.0f) {
			twist = { -twist.x, -twist.y, -twist.z, -twist.w };
		}
		return twist;
	}

	// q scaled to a fraction of its angle about the same axis (a_fraction may be negative).
	inline Quat Scaled(const Quat& q, float a_fraction)
	{
		const float s = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z);
		if (s < 1e-6f) {
			return {};
		}
		const float half = std::atan2(s, q.w) * a_fraction;
		const float k = std::sin(half) / s;
		return { q.x * k, q.y * k, q.z * k, std::cos(half) };
	}

	// NiMatrix3 is entry[row][col] with world = M * local.
	inline RE::NiMatrix3 ToMatrix(const Quat& q)
	{
		RE::NiMatrix3 m;
		const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
		const float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
		const float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
		m.entry[0][0] = 1 - 2 * (yy + zz);
		m.entry[0][1] = 2 * (xy - wz);
		m.entry[0][2] = 2 * (xz + wy);
		m.entry[1][0] = 2 * (xy + wz);
		m.entry[1][1] = 1 - 2 * (xx + zz);
		m.entry[1][2] = 2 * (yz - wx);
		m.entry[2][0] = 2 * (xz - wy);
		m.entry[2][1] = 2 * (yz + wx);
		m.entry[2][2] = 1 - 2 * (xx + yy);
		return m;
	}

	inline Quat ToQuat(const RE::NiMatrix3& m)
	{
		const auto& e = m.entry;
		Quat q;
		const float tr = e[0][0] + e[1][1] + e[2][2];
		if (tr > 0.0f) {
			const float s = std::sqrt(tr + 1.0f) * 2.0f;
			q = { (e[2][1] - e[1][2]) / s, (e[0][2] - e[2][0]) / s, (e[1][0] - e[0][1]) / s, 0.25f * s };
		} else if (e[0][0] > e[1][1] && e[0][0] > e[2][2]) {
			const float s = std::sqrt(1.0f + e[0][0] - e[1][1] - e[2][2]) * 2.0f;
			q = { 0.25f * s, (e[0][1] + e[1][0]) / s, (e[0][2] + e[2][0]) / s, (e[2][1] - e[1][2]) / s };
		} else if (e[1][1] > e[2][2]) {
			const float s = std::sqrt(1.0f + e[1][1] - e[0][0] - e[2][2]) * 2.0f;
			q = { (e[0][1] + e[1][0]) / s, 0.25f * s, (e[1][2] + e[2][1]) / s, (e[0][2] - e[2][0]) / s };
		} else {
			const float s = std::sqrt(1.0f + e[2][2] - e[0][0] - e[1][1]) * 2.0f;
			q = { (e[0][2] + e[2][0]) / s, (e[1][2] + e[2][1]) / s, 0.25f * s, (e[1][0] - e[0][1]) / s };
		}
		return Normalized(q);
	}

	// Shortest rotation taking unit vector a onto unit vector b.
	inline Quat Between(const RE::NiPoint3& a, const RE::NiPoint3& b)
	{
		const float d = a.x * b.x + a.y * b.y + a.z * b.z;
		if (d < -0.9999f) {
			// opposite: any axis perpendicular to a
			RE::NiPoint3 axis = std::abs(a.x) < 0.9f ? RE::NiPoint3{ 0.0f, -a.z, a.y } : RE::NiPoint3{ -a.z, 0.0f, a.x };
			const float n = std::sqrt(axis.x * axis.x + axis.y * axis.y + axis.z * axis.z);
			return { axis.x / n, axis.y / n, axis.z / n, 0.0f };
		}
		return Normalized({ a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x, 1.0f + d });
	}
}
