Texture2D<float4> colorTex : register(t0);
Texture2D<float2> mapTex : register(t1);
Texture2D<uint> depthTex : register(t2);
Texture2D<uint> infraredTex : register(t3);
SamplerState samColor : register(s0);

static const float k1 = 8.5154194367447283e-002;
static const float k2 = -2.6706920315697513e-001;
static const float k3 = 9.9116038520186175e-002;
static const float p1 = 3.0158565240179448e-003;
static const float p2 = -1.3463984002856915e-003;

static const float fx = 3.6297160236896798e+002;
static const float fy = 3.6380609025908501e+002;
static const float cx = 2.5550000000000000e+002;
static const float cy = 2.1150000000000000e+002;

struct GS_INPUT{};
struct PS_INPUT{
	float4 Pos : SV_POSITION;
	float2 Tex : TEXCOORD0;
};
GS_INPUT VS(){
	GS_INPUT output = (GS_INPUT)0;
	return output;
}
[maxvertexcount(4)]
void GS( point GS_INPUT particles[1], inout TriangleStream<PS_INPUT> triStream ){
	PS_INPUT output;
	float scaler = 1.2f;
	output.Pos = float4( -1.0f, 1.0f, 0.01f, 1.0f );
	output.Tex = float2( 0.5f, 0.5f ) + float2( -0.5f, -0.5f ) * scaler;
	triStream.Append( output );
	output.Pos = float4( 1.0f, 1.0f, 0.01f, 1.0f );
	output.Tex = float2( 0.5f, 0.5f ) + float2( 0.5f, -0.5f ) * scaler;
	triStream.Append( output );
	output.Pos = float4( -1.0f, -1.0f, 0.01f, 1.0f );
	output.Tex = float2( 0.5f, 0.5f ) + float2( -0.5f, 0.5f ) * scaler;
	triStream.Append( output );
	output.Pos = float4( 1.0f, -1.0f, 0.01f, 1.0f );
	output.Tex = float2( 0.5f, 0.5f ) + float2( 0.5f, 0.5f ) * scaler;
	triStream.Append( output );
	/*output.Pos = float4( -1.0f, 1.0f, 0.01f, 1.0f );
	output.Tex = float2( 0.0f, 0.0f );
	triStream.Append( output );
	output.Pos = float4( 1.0f, 1.0f, 0.01f, 1.0f );
	output.Tex = float2( 1.0f, 0.0f );
	triStream.Append( output );
	output.Pos = float4( -1.0f, -1.0f, 0.01f, 1.0f );
	output.Tex = float2( 0.0f, 1.0f );
	triStream.Append( output );
	output.Pos = float4( 1.0f, -1.0f, 0.01f, 1.0f );
	output.Tex = float2( 1.0f, 1.0f );
	triStream.Append( output );*/
}
float4 PS(PS_INPUT input) : SV_Target{
	float2 idx = input.Tex * float2(512,424) ;
	idx.x = (idx.x - cx) / fx;
	idx.y = (idx.y - cy) / fy;
	float r2 = dot(idx,idx);
	float r4 = r2 * r2;
	float r6 = r2 * r4;

	float2 nidx;
	nidx.x = idx.x*(1.f + k1*r2 + k2*r4 + k3*r6) + 2.f*p1*idx.x*idx.y + p2*(r2 + 2.f*idx.x*idx.x);
	nidx.y = idx.y*(1.f + k1*r2 + k2*r4 + k3*r6) + 2.f*p2*idx.x*idx.y + p1*(r2 + 2.f*idx.y*idx.y);

	nidx.x = nidx.x * fx + cx;
	nidx.y = nidx.y * fy + cy;

	float2 rawData = mapTex.Sample(samColor,nidx / float2(512,424))/float2(1920,1080);
	float4 color = colorTex.Sample(samColor,rawData);
	uint depth = depthTex.Load(int3(nidx,0));
	color.w = depth/1000.f;
	return color;
}