#pragma once
#include "utils/math/Vector.h"
#include <cstdint>
#include <map>
#include <vector>
#include <string>
//================================
// 行列・変換系の構造体
//================================
struct Matrix3x3
{
    float m[3][3];
};

struct Matrix4x4
{
    float m[4][4];
};

struct EulerTransform
{
    Vector3 scale;
    Vector3 rotate;
    Vector3 translate;
};

using Transform = EulerTransform;

struct Quaternion
{
    float x;
    float y;
    float z;
    float w;
};

struct QuaternionTransform
{
    Vector3 scale;
    Quaternion rotate;
    Vector3 translate;
};

struct Sphere
{
    Vector3 center;
    float   radius;
};

struct TransformationMatrix
{
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
};

//================================
// マテリアル / ライト（★ここに移動）
//================================
struct Material
{
    Vector4  color;
    int32_t  enableLighting;
    float    padding[3];
    Matrix4x4 uvTransform;
    float shininess;
    float environmentCoefficient;
    int32_t specularMode; // 0: Phong, 1: Blinn-Phong
    float padding2[1];
};

struct DirectionalLight
{
    Vector4 color;      ///< ライトの色
    Vector3 direction;  ///< ライトの向き（単位ベクトル）
    float   intensity;  ///< 輝度（明るさ）
};

struct PointLight {
    Vector4 color;
    Vector3 position;
    float intensity;
    float radius;      // 4
    float decay;       // 4
    float padding[2];  // 8 -> 合計48（16の倍数）
};

struct SpotLight
{
    Vector4 color;
    Vector3 position;
    float intensity;

    Vector3 direction;
    float distance;

    float decay;
    float cosAngle;
    float padding[2]; // 16byte alignment
};

// MaterialData構造体
struct MaterialData {
    std::string textureFilePath;
};

struct VertexWeightData {
    float weight = 0.0f;
    uint32_t vertexIndex = 0;
};

struct JointWeightData {
    Matrix4x4 inverseBindPoseMatrix{};
    std::vector<VertexWeightData> vertexWeights;
};

struct Node {
    QuaternionTransform transform;
    Matrix4x4 localMatrix;        // Nodeのローカル変換
    std::string name;             // Node名
    std::vector<Node> children;   // 子
};

// ModelData構造体
struct ModelData {
    std::vector<VertexData> vertices;
    std::vector<uint32_t> indices;
    std::map<std::string, JointWeightData> skinClusterData;
    MaterialData material;
    Node rootNode;
};




//================================
// 関数プロトタイプ
//================================
Matrix4x4 MakeIdentity4x4();
Matrix4x4 Multiply(Matrix4x4 m1, Matrix4x4 m2);
Matrix4x4 Inverse(Matrix4x4& m);
Matrix4x4 Transpose(const Matrix4x4& mat);
Vector3   Normalize(const Vector3& v);
Quaternion Normalize(const Quaternion& q);
Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t);
//Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);
//Matrix3x3 MakeScaleMatrix(const Vector2& scale);
//Matrix3x3 MakeTranslateMatrix(const Vector2& translate);
Matrix4x4 MakeScaleMatrix(const Vector3& scale);
Matrix4x4 MakeTranslateMatrix(const Vector3& translate);
Matrix3x3 MakeRotateZMatrix(float angleRad);
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);
Matrix4x4 MakeRoateXMatrix(float radian);
Matrix4x4 MakeRoateYMatrix(float radian);
Matrix4x4 MakeRoateZMatrix(float radian);
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
Matrix4x4 MakeRotateMatrix(const Quaternion& rotate);
Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);
