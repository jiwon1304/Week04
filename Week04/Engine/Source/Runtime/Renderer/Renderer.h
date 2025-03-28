#pragma once
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

#define _TCHAR_DEFINED
#include <d3d11.h>
#include <array>
#include "EngineBaseTypes.h"
#include "Define.h"
#include "Container/Map.h"
#include "Container/Set.h"

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

// sorting
constexpr float OCCLUSION_DISTANCE_DIV = 10.0f;  // 적절한 초기값으로 초기화
constexpr int OCCLUSION_DISTANCE_BIN_NUM = 100/ OCCLUSION_DISTANCE_DIV+2;  // +1 : 나눗셈 나머지 , +2 넘어가는것들
constexpr size_t OcclusionBufferSizeWidth = 256;
constexpr size_t OcclusionBufferSizeHeight = 256;

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
    ID3D11Buffer* LightingBuffer = nullptr;
    ID3D11Buffer* FlagBuffer = nullptr;
    ID3D11Buffer* MaterialConstantBuffer = nullptr;
    ID3D11Buffer* SubMeshConstantBuffer = nullptr;
    ID3D11Buffer* TextureConstantBufer = nullptr;

    FLighting lightingData;

    uint32 Stride;
    uint32 Stride2;

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

    // update
    void UpdateLightBuffer() const;
    void UpdateConstant(const FMatrix& MVP, FVector4 UUIDColor, bool IsSelected) const;
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
    void ClearRenderArr();
    void Render();
    void RenderStaticMeshes();
    void RenderGizmos();
    void RenderLight();
    void RenderBillboards();

    static bool SortActorArray(const MeshMaterialPair& a, const MeshMaterialPair& b);
    
private:
    std::vector<MeshMaterialPair> SortedStaticMeshObjs;

    struct FMeshData // 렌더러 내부에서만 사용하므로 여기에서 선언
    {
        UStaticMeshComponent* StaticMeshComp;
        uint32 SubMeshIndex;
        uint32 IndexStart;
        uint32 IndexCount;
    };

    /**
     * Key: 머티리얼
     * Value: 해당 머티리얼을 사용하는 서브메시의 배열
     */
    TMap<UMaterial*, TArray<FMeshData>> MaterialMeshMap; 

    // 스태틱메시를 기준으로 정렬하여 버텍스 버퍼와 인덱스 버퍼를 바꾸는 횟수를 최소화하기 위함
    static bool SubmeshCmp(const FMeshData& a, const FMeshData& b);
    
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
    std::array<TArray<UStaticMeshComponent*>, OCCLUSION_DISTANCE_BIN_NUM> SortedMeshes;
    void SortMeshRoughly();
    //std::array<TArray<UStaticMeshComponent*>, DISTANCE_BIN_NUM> ProcessChunk(TObjectIterator<UStaticMeshComponent> startIter, size_t endIdx);
    //void ProcessMeshesInParallel();


    TArray<UStaticMeshComponent*> DisOccludedMeshes;
    void OcclusionCulling();
    std::array<uint32_t, OcclusionBufferSizeWidth/sizeof(uint32_t) * OcclusionBufferSizeHeight / sizeof(uint32_t)> OcclusionBuffer;
};

