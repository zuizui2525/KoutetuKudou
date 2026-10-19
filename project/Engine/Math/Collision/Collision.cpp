#include "Engine/Math/Collision/Collision.h"
#include "Engine/Math/Matrix/Matrix.h"
#include <algorithm>

using namespace Math;

// 球同士の当たり判定
bool IsCollision(const Sphere& sphere1, const Sphere& sphere2) {
	float distance = Length(Subtract(sphere1.center, sphere2.center));
	return distance <= (sphere1.radius + sphere2.radius);
}

// 球と面の当たり判定
bool IsCollision(const Sphere& sphere, const Plane& plane) {
	float signedDistance = Dot(plane.normal, sphere.center) - plane.distance;
	return fabs(signedDistance) <= sphere.radius;
}

// 線と平面の当たり判定
bool IsCollision(const Segment& segment, const Plane& plane) {
	Vector3 lineDir = segment.diff; // 線の方向ベクトル
	float dot = Dot(plane.normal, lineDir);
	if (dot == 0.0f) { // 線と平面が平行な場合
		return false;
	}
	float t = (plane.distance - Dot(plane.normal, segment.origin)) / dot;
	if (t < 0.0f || t > 1.0f) { // 線分の範囲外
		return false;
	}
	return true;
}

// 線と三角形の当たり判定
bool IsCollision(const Segment& segment, const Triangle& triangle) {
	Vector3 v0 = triangle.vertex[0];
	Vector3 v1 = triangle.vertex[1];
	Vector3 v2 = triangle.vertex[2];

	// 三角形の法線ベクトルを計算
	Vector3 edge1 = Subtract(v1, v0);
	Vector3 edge2 = Subtract(v2, v0);
	Vector3 normal = Cross(edge1, edge2);

	// 平面との交点を求める
	float dot = Dot(normal, segment.diff);
	if (dot == 0.0f) return false; // 平行なので衝突しない

	float t = Dot(normal, Subtract(v0, segment.origin)) / dot;
	if (t < 0.0f || t > 1.0f) return false; // 線分の範囲外

	// 衝突点を求める
	Vector3 closestPoint = Add(segment.origin, Multiply(t, segment.diff));

	// 内積を使って交点が三角形の内側にあるか判定
	Vector3 v0p = Subtract(closestPoint, v0);
	Vector3 v1p = Subtract(closestPoint, v1);
	Vector3 v2p = Subtract(closestPoint, v2);

	Vector3 v01 = Subtract(v1, v0);
	Vector3 v12 = Subtract(v2, v1);
	Vector3 v20 = Subtract(v0, v2);

	Vector3 cross01 = Cross(v01, v0p);
	Vector3 cross12 = Cross(v12, v1p);
	Vector3 cross20 = Cross(v20, v2p);

	if (Dot(cross01, normal) >= 0.0f &&
		Dot(cross12, normal) >= 0.0f &&
		Dot(cross20, normal) >= 0.0f) {
		return true;
	}

	return false;
}

// AABB同士の当たり判定
bool IsCollision(const AABB& a, const AABB& b) {
	return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
		(a.min.y <= b.max.y && a.max.y >= b.min.y) &&
		(a.min.z <= b.max.z && a.max.z >= b.min.z);
}

// AABB同士の重なりを解消するための押し出しベクトル（bをaから離すベクトル）を計算
bool CalculateAABBPushVector(const AABB& a, const AABB& b, Vector3& outPushVector) {
	outPushVector = { 0.0f, 0.0f, 0.0f };
	if (!IsCollision(a, b)) {
		return false;
	}

	// 各軸の重なり幅を計算
	float overlapX = (std::min)(a.max.x, b.max.x) - (std::max)(a.min.x, b.min.x);
	float overlapY = (std::min)(a.max.y, b.max.y) - (std::max)(a.min.y, b.min.y);
	float overlapZ = (std::min)(a.max.z, b.max.z) - (std::max)(a.min.z, b.min.z);

	if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f) {
		return false;
	}

	// 中心座標から押し出し方向（離脱符号）を判定
	constexpr float kHalf = 0.5f;
	float centerAX = (a.min.x + a.max.x) * kHalf;
	float centerAZ = (a.min.z + a.max.z) * kHalf;
	float centerBX = (b.min.x + b.max.x) * kHalf;
	float centerBZ = (b.min.z + b.max.z) * kHalf;

	float signX = (centerBX >= centerAX) ? 1.0f : -1.0f;
	float signZ = (centerBZ >= centerAZ) ? 1.0f : -1.0f;

	// ゲームプレイ上、水平面（XZ）で最も侵入の浅い軸に沿って押し出す
	if (overlapX < overlapZ) {
		outPushVector.x = overlapX * signX;
	} else {
		outPushVector.z = overlapZ * signZ;
	}

	return true;
}

// AABBと点の当たり判定
bool IsCollision(const AABB& aabb, const Vector3& position) {
	return (position.x >= aabb.min.x && position.x <= aabb.max.x) &&
		(position.y >= aabb.min.y && position.y <= aabb.max.y) &&
		(position.z >= aabb.min.z && position.z <= aabb.max.z);
}

// AABBと球の当たり判定
bool IsCollision(const AABB& aabb, const Sphere& sphere) {
	Vector3 closestPoint{
	std::clamp(sphere.center.x, aabb.min.x, aabb.max.x),
	std::clamp(sphere.center.y, aabb.min.y, aabb.max.y),
	std::clamp(sphere.center.z, aabb.min.z, aabb.max.z)
	};
	float distance = Length(Subtract(closestPoint, sphere.center));
	if (distance <= sphere.radius) {
		return true;
	} else {
		return false;
	}
}

// AABBと線の当たり判定
bool IsCollision(const AABB& aabb, const Segment& segment) {
	// tmin: 線分がAABBに入り始める時刻（パラメータ）
	// tmax: 線分がAABBから出る時刻
	float tmin = 0.0f;
	float tmax = 1.0f;

	// 線分の始点と方向（差分ベクトル）
	Vector3 p = segment.origin;
	Vector3 d = segment.diff;

	// X, Y, Zの3軸それぞれについてチェック
	for (int i = 0; i < 3; ++i) {
		float origin, dir, min, max;

		// iに応じて現在の軸の値を取得
		if (i == 0) {
			origin = p.x; dir = d.x;
			min = aabb.min.x; max = aabb.max.x;
		} else if (i == 1) {
			origin = p.y; dir = d.y;
			min = aabb.min.y; max = aabb.max.y;
		} else {
			origin = p.z; dir = d.z;
			min = aabb.min.z; max = aabb.max.z;
		}

		// 線分がこの軸に平行な場合
		if (fabs(dir) < 1e-6f) {
			// 始点がAABBの範囲外なら交差しない
			if (origin < min || origin > max) {
				return false;
			}
		} else {
			// dirが0でない場合（傾いている軸）

			// 逆数をとって計算を高速化
			float ood = 1.0f / dir;

			// AABBの最小面・最大面との交差時刻を計算
			float t1 = (min - origin) * ood;
			float t2 = (max - origin) * ood;

			// t1がt2より大きいなら入れ替える（昇順に）
			if (t1 > t2) std::swap(t1, t2);

			// tminを更新（他の軸も含めた最も遅い進入時刻）
			tmin = (std::max)(tmin, t1);

			// tmaxを更新（他の軸も含めた最も早い脱出時刻）
			tmax = (std::min)(tmax, t2);

			// 交差が成立しない場合（入る前に出る）
			if (tmin > tmax) {
				return false;
			}
		}
	}

	// 全軸において交差範囲が重なっている → 当たり判定あり
	return true;
}

// OBBと球の当たり判定
bool IsCollision(const OBB& obb, const Sphere& sphere) {
	// 円中心 → OBB中心ベクトル
	Vector3 d = Subtract(sphere.center, obb.center);

	// OBBのローカル座標系におけるsphere中心の位置を求める
	float localX = Dot(d, obb.orientations[0]);
	float localY = Dot(d, obb.orientations[1]);
	float localZ = Dot(d, obb.orientations[2]);

	// AABBとしてclamp（最近接点をローカル空間上で求める）
	float clampedX = (std::max)(-obb.size.x, (std::min)(localX, obb.size.x));
	float clampedY = (std::max)(-obb.size.y, (std::min)(localY, obb.size.y));
	float clampedZ = (std::max)(-obb.size.z, (std::min)(localZ, obb.size.z));

	// 最近接点をワールド座標に変換
	Vector3 closestPoint = obb.center;
	closestPoint = Add(closestPoint, Multiply(clampedX, obb.orientations[0]));
	closestPoint = Add(closestPoint, Multiply(clampedY, obb.orientations[1]));
	closestPoint = Add(closestPoint, Multiply(clampedZ, obb.orientations[2]));

	// 球の中心と最近接点との距離²を求める
	Vector3 diff = Subtract(closestPoint, sphere.center);
	float distanceSq = Dot(diff, diff);

	// 衝突判定：距離² <= 半径²
	return distanceSq <= sphere.radius * sphere.radius;
}

// OBBと線の当たり判定
bool IsCollision(const OBB& obb, const Segment& segment) {
	// 1. 線分をOBBのローカル空間に変換

	// OBBの中心から線分始点へのベクトル
	Vector3 relOrigin = Subtract(segment.origin, obb.center);

	// ローカル空間への変換（軸に射影）
	Vector3 pLocal = {
		Dot(relOrigin, obb.orientations[0]),
		Dot(relOrigin, obb.orientations[1]),
		Dot(relOrigin, obb.orientations[2]),
	};
	Vector3 dLocal = {
		Dot(segment.diff, obb.orientations[0]),
		Dot(segment.diff, obb.orientations[1]),
		Dot(segment.diff, obb.orientations[2]),
	};

	// 2. AABBと同様のスラブ法で交差判定
	float tmin = 0.0f;
	float tmax = 1.0f;

	for (int i = 0; i < 3; ++i) {
		float origin, dir, min, max;

		// iに応じて現在の軸の値を取得
		if (i == 0) {
			origin = pLocal.x; dir = dLocal.x;
			min = -obb.size.x; max = obb.size.x;
		} else if (i == 1) {
			origin = pLocal.y; dir = dLocal.y;
			min = -obb.size.y; max = obb.size.y;
		} else {
			origin = pLocal.z; dir = dLocal.z;
			min = -obb.size.z; max = obb.size.z;
		}

		if (fabs(dir) < 1e-6f) {
			// 線分がこの軸と平行 → 中心が範囲外なら衝突なし
			if (origin < min || origin > max) return false;
		} else {
			float ood = 1.0f / dir;
			float t1 = (min - origin) * ood;
			float t2 = (max - origin) * ood;
			if (t1 > t2) std::swap(t1, t2);

			tmin = (std::max)(tmin, t1);
			tmax = (std::min)(tmax, t2);

			if (tmin > tmax) return false;
		}
	}

	// 線分とOBBが交差している
	return true;
}
