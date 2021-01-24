

/**************************************************************************************************
 *** This file was autogenerated from GrSectionHeader.fp; do not modify.
 **************************************************************************************************/
#include "GrSectionHeader.h"

#include "src/core/SkUtils.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/sksl/SkSLCPP.h"
#include "src/sksl/SkSLUtil.h"
class GrGLSLSectionHeader : public GrGLSLFragmentProcessor {
public:
    GrGLSLSectionHeader() {}
    void emitCode(EmitArgs& args) override {
        GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
        const GrSectionHeader& _outer = args.fFp.cast<GrSectionHeader>();
        (void) _outer;
        fragBuilder->codeAppendf(
R"SkSL(return half4(1.0);
)SkSL"
);
    }
private:
    void onSetData(const GrGLSLProgramDataManager& pdman, const GrFragmentProcessor& _proc) override {
    }
};
GrGLSLFragmentProcessor* GrSectionHeader::onCreateGLSLInstance() const {
    return new GrGLSLSectionHeader();
}
void GrSectionHeader::onGetGLSLProcessorKey(const GrShaderCaps& caps, GrProcessorKeyBuilder* b) const {
}
bool GrSectionHeader::onIsEqual(const GrFragmentProcessor& other) const {
    const GrSectionHeader& that = other.cast<GrSectionHeader>();
    (void) that;
    return true;
}
GrSectionHeader::GrSectionHeader(const GrSectionHeader& src)
: INHERITED(kGrSectionHeader_ClassID, src.optimizationFlags()) {
        this->cloneAndRegisterAllChildProcessors(src);
}
std::unique_ptr<GrFragmentProcessor> GrSectionHeader::clone() const {
    return std::make_unique<GrSectionHeader>(*this);
}
#if GR_TEST_UTILS
SkString GrSectionHeader::onDumpInfo() const {
    return SkString();
}
#endif
