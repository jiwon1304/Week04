#pragma once
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#define _TCHAR_DEFINED
#include <d3d11.h>
#include "EngineBaseTypes.h"
#include "Define.h"
#include "Container/Map.h"
#include "Container/Set.h"

// sorting
#include <array>
#include <queue>

constexpr float OCCLUSION_DISTANCE_DIV = 1.1f;  // 적절한 초기값으로 초기화
constexpr int OCCLUSION_DISTANCE_BIN_NUM = 49+1;  // log(1.2f)^100
constexpr int OcclusionBufferSizeWidth = 256;
constexpr int OcclusionBufferSizeHeight = 256;


class UStaticMesh;
class ULightComponentBase;
class UWorld;
class FGraphicsDevice;
class UMaterial;
struct FStaticMaterial;
class UObject;
class FEditorViewportClient;
class UBillboardComponent;
class UStaticMeshComponent;
class UGizmoBaseComponent;
class FRenderer 
{

private:
    float litFlag = 0;
public:
    FGraphicsDevice* Graphics;
    ID3D11VertexShader* VertexShader = nullptr;
    ID3D11PixelShader* PixelShader = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11Buffer* ConstantBuffer = nullptr;

    ID3D11Buffer* ConstantBufferView = nullptr;
    ID3D11Buffer* ConstantBufferProjection = nullptr;
    
    ID3D11Buffer* LightingBuffer = nullptr;
    ID3D11Buffer* FlagBuffer = nullptr;
    ID3D11Buffer* MaterialConstantBuffer = nullptr;
    ID3D11Buffer* SubMeshConstantBuffer = nullptr;
    ID3D11Buffer* TextureConstantBuffer = nullptr;

    FLighting lightingData;

    uint32 Stride;
    uint32 Stride2;
    uint32 StriedOcclusion;

public:
    void Initialize(FGraphicsDevice* graphics);
   
    void PrepareShader() const;
    
    //Render
    void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices) const;
    void RenderPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices) const;
    void RenderPrimitive(OBJ::FStaticMeshRenderData* renderData, TArray<FStaticMaterial*> materials, TArray<UMaterial*> overrideMaterial, int selectedSubMeshIndex) const;
   
    void RenderTexturedModelPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices, ID3D11Buffer* pIndexBuffer, UINT numIndices, ID3D11ShaderResourceView* InTextureSRV, ID3D11SamplerState* InSamplerState) const;
    //Release
    void Release();
    void ReleaseShader();
    void ReleaseBuffer(ID3D11Buffer*& Buffer) const;
    void ReleaseConstantBuffer();

    void ResetVertexShader() const;
    void ResetPixelShader() const;
    void CreateShader();

    void SetVertexShader(const FWString& filename, const FString& funcname, const FString& version);
    void SetPixelShader(const FWString& filename, const FString& funcname, const FString& version);
    
    void ChangeViewMode(EViewModeIndex evi) const;
    
    // CreateBuffer
    void CreateConstantBuffer();
    void CreateLightingBuffer();
    void CreateLitUnlitBuffer();
    ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth) const;
    ID3D11Buffer* CreateVertexBuffer(const TArray<FVertexSimple>& vertices, UINT byteWidth) const;
    ID3D11Buffer* CreateIndexBuffer(uint32* indices, UINT byteWidth) const;
    ID3D11Buffer* CreateIndexBuffer(const TArray<uint32>& indices, UINT byteWidth) const;

    // Setup
    void BindBuffers();
    
    // update
    void UpdateLightBuffer() const;
    void UpdateConstant(const FMatrix& WorldMatrix, FVector4 UUIDColor, bool IsSelected) const;

    void UpdateViewMatrix(const FMatrix& ViewMatrix) const;
    void UpdateProjectionMatrix(const FMatrix& ProjectionMatrix) const;
    
    void UpdateMaterial(const FObjMaterialInfo& MaterialInfo) const;
    void UpdateLitUnlitConstant(int isLit) const;
    void UpdateIsGizmoConstant(int IsGizmo) const;
    void UpdateSubMeshConstant(bool isSelected) const;
    void UpdateTextureConstant(float UOffset, float VOffset);

public://텍스쳐용 기능 추가
    ID3D11VertexShader* VertexTextureShader = nullptr;
    ID3D11PixelShader* PixelTextureShader = nullptr;
    ID3D11InputLayout* TextureInputLayout = nullptr;

    uint32 TextureStride;
    struct FSubUVConstant
    {
        float indexU;
        float indexV;
    };
    ID3D11Buffer* SubUVConstantBuffer = nullptr;

public:
    void CreateTextureShader();
    void ReleaseTextureShader();
    void PrepareTextureShader() const;
    ID3D11Buffer* CreateVertexTextureBuffer(FVertexTexture* vertices, UINT byteWidth) const;
    ID3D11Buffer* CreateIndexTextureBuffer(uint32* indices, UINT byteWidth) const;
    void RenderTexturePrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices,
        ID3D11Buffer* pIndexBuffer, UINT numIndices,
        ID3D11ShaderResourceView* _TextureSRV,
        ID3D11SamplerState* _SamplerState) const;
    void RenderTextPrimitive(ID3D11Buffer* pVertexBuffer, UINT numVertices,
        ID3D11ShaderResourceView* _TextureSRV,
        ID3D11SamplerState* _SamplerState) const;
    ID3D11Buffer* CreateVertexBuffer(FVertexTexture* vertices, UINT byteWidth) const;

    void UpdateSubUVConstant(float _indexU, float _indexV) const;
    void PrepareSubUVConstant() const;


public: // line shader
    void PrepareLineShader() const;
    void CreateLineShader();
    void ReleaseLineShader() const;
    void RenderBatch(const FGridParameters& gridParam, ID3D11Buffer* pVertexBuffer, int boundingBoxCount, int coneCount, int coneSegmentCount, int obbCount) const;
    void UpdateGridConstantBuffer(const FGridParameters& gridParams) const;
    void UpdateLinePrimitveCountBuffer(int numBoundingBoxes, int numCones) const;
    ID3D11Buffer* CreateStaticVerticesBuffer() const;
    ID3D11Buffer* CreateBoundingBoxBuffer(UINT numBoundingBoxes) const;
    ID3D11Buffer* CreateOBBBuffer(UINT numBoundingBoxes) const;
    ID3D11Buffer* CreateConeBuffer(UINT numCones) const;
    ID3D11ShaderResourceView* CreateBoundingBoxSRV(ID3D11Buffer* pBoundingBoxBuffer, UINT numBoundingBoxes);
    ID3D11ShaderResourceView* CreateOBBSRV(ID3D11Buffer* pBoundingBoxBuffer, UINT numBoundingBoxes);
    ID3D11ShaderResourceView* CreateConeSRV(ID3D11Buffer* pConeBuffer, UINT numCones);

    void UpdateBoundingBoxBuffer(ID3D11Buffer* pBoundingBoxBuffer, const TArray<FBoundingBox>& BoundingBoxes, int numBoundingBoxes) const;
    void UpdateOBBBuffer(ID3D11Buffer* pBoundingBoxBuffer, const TArray<FOBB>& BoundingBoxes, int numBoundingBoxes) const;
    void UpdateConesBuffer(ID3D11Buffer* pConeBuffer, const TArray<FCone>& Cones, int numCones) const;

    //Render Pass Demo
    void SetViewport(std::shared_ptr<FEditorViewportClient> InActiveViewport);
    void SetWorld(UWorld* InWorld);
    void PrepareRender();
    bool IsInsideFrustum(UStaticMeshComponent* StaticMeshComp) const;
    void ClearRenderArr();
    void Render();
    void RenderStaticMeshes();
    void RenderGizmos();
    void RenderLight();
    void RenderBillboards();

private:
    std::vector<MeshMaterialPair> SortedStaticMeshObjs;

    struct FMeshData // 렌더러 내부에서만 사용하므로 여기에서 선언
    {
        uint32 SubMeshIndex;
        uint32 IndexStart;
        uint32 IndexCount;
        FMatrix WorldMatrix;
        FVector4 EncodeUUID;
        bool bIsSelected;
    };

    /**
     * Key: 머티리얼
     * Value: 해당 머티리얼을 사용하는 서브메시의 배열
     */
    std::unordered_map<UMaterial*, std::unordered_map<UStaticMesh*, std::vector<FMeshData>>> MaterialMeshMap; 

    TArray<UGizmoBaseComponent*> GizmoObjs;
    TArray<UBillboardComponent*> BillboardObjs;
    TArray<ULightComponentBase*> LightObjs;

public:
    ID3D11VertexShader* VertexLineShader = nullptr;
    ID3D11PixelShader* PixelLineShader = nullptr;
    ID3D11Buffer* GridConstantBuffer = nullptr;
    ID3D11Buffer* LinePrimitiveBuffer = nullptr;
    ID3D11ShaderResourceView* pBBSRV = nullptr;
    ID3D11ShaderResourceView* pConeSRV = nullptr;
    ID3D11ShaderResourceView* pOBBSRV = nullptr;

private:
    std::shared_ptr<FEditorViewportClient> ActiveViewport;
    UWorld* World = nullptr;



private:
    TArray<UStaticMeshComponent*> FrustumMeshes;
    std::array<TArray<UStaticMeshComponent*>, OCCLUSION_DISTANCE_BIN_NUM> MeshesSortedByDistance;
    void SortMeshRoughly();
    //std::array<TArray<UStaticMeshComponent*>, DISTANCE_BIN_NUM> ProcessChunk(TObjectIterator<UStaticMeshComponent> startIter, size_t endIdx);
    //void ProcessMeshesInParallel();


    TArray<UStaticMeshComponent*> DisOccludedMeshes;
    //void OcclusionCulling();
    //void OcclusionCullingGPU();
    // TODO : Week04 SIMD 쓸때 사용
    //std::array<uint32_t, OcclusionBufferSizeWidth/sizeof(uint32_t) * OcclusionBufferSizeHeight / sizeof(uint32_t)> OcclusionBuffer;
    //std::array<bool, OcclusionBufferSizeWidth / sizeof(bool) * OcclusionBufferSizeHeight / sizeof(bool)> OcclusionBufferBinary;

private:
    void InitOcclusionQuery();
    void PrepareOcclusion();
    void QueryOcclusion();
    ID3D11Query* OcclusionQuery = nullptr;
    ID3D11Texture2D* OcclusionTexture = nullptr;
    ID3D11DepthStencilView* OcclusionDSV = nullptr;

    ID3D11DepthStencilState* OcclusionWriteZero = nullptr;
    ID3D11DepthStencilState* OcclusionWriteAlways = nullptr;

    //안씀
    ID3D11ShaderResourceView* OcclusionSRV = nullptr;
    ID3D11UnorderedAccessView* OcclusionUAV = nullptr;
    ID3D11ComputeShader* OcclusionComputeShader = nullptr;
    ID3D11Buffer* OcclusionBuffer = nullptr;

    void RenderOccluder(UStaticMeshComponent* StaticMeshComp);
    void RenderOccludee(UStaticMeshComponent* StaticMeshComp);
    void RenderObjectsForOcclusionQuery(TArray<UStaticMeshComponent*> MeshComps);


    void CreateOcclusionShader();
    ID3D11VertexShader* OcclusionVertexShader = nullptr;
    ID3D11PixelShader* OcclusionPixelShader = nullptr;

    //ID3D11InputLayout* OcclusionInputLayout = nullptr;
    ID3D11Buffer* OcclusionConstantBuffer = nullptr;
    ID3D11Buffer* OcclusionObjectInfoBuffer = nullptr;

    D3D11_QUERY_DESC queryDesc;
    std::queue<ID3D11Query*> QueryPool;
    std::queue<ID3D11Query*> Queries;
    std::queue<UStaticMeshComponent*> QueryMeshes;
};

