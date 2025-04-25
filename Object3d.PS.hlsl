struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main()
{
    PixelShaderOutput output;
    output.color = {
        1.0, 1.0, 1.0, 1.0
    }
    return output;

}