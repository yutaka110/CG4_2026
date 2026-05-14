struct PixelInput {
    float32_t4 position : SV_POSITION;
    float32_t4 color : COLOR0;
};

float32_t4 main(PixelInput input) : SV_TARGET0 {
    return input.color;
}
