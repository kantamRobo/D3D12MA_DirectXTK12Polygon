struct VS_INPUT
{
    float4 pos : POSITION;   // ���_�ʒu
   
};

cbuffer ConstantBuffer : register(b1)
{
    float4x4 World;         // ���[���h�ϊ��s��
    float4x4 View;          // �r���[�ϊ��s��
    float4x4 Projection;    // �����ˉe�ϊ��s��
};

struct VS_OUTPUT
{
    float4 pos : SV_POSITION; // �o�͂̃X�N���[�����W
   
};

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT output;
    
    // ���[���h�ϊ��A�r���[�ϊ��A�v���W�F�N�V�����ϊ���K�p
    float4 worldPosition = mul(input.pos, World);
    float4 viewPosition = mul(worldPosition, View);
    
    output.pos = mul(viewPosition, Projection);
    

   


    return output;
}
