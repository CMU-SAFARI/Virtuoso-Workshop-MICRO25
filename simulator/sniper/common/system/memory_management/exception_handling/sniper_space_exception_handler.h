#include "misc/exception_handler_base.h"

class SniperExceptionHandler : public ExceptionHandlerBase {
private:
    std::ofstream log_file;
public:
    SniperExceptionHandler(Core* core);
    ~SniperExceptionHandler();

    void handle_exception(int exception_type_code, int argc, uint64_t *argv) override;
    void handle_page_fault(FaultCtx& ctx) override;
    PhysicalMemoryAllocator* getAllocator() override; 

    ExceptionHandlerBase::FaultCtx initFaultCtx(ParametricDramDirectoryMSI::PageTable* page_table, UInt64 address, UInt64 core_id, int max_level) {
            ExceptionHandlerBase::FaultCtx fault_ctx;
            std::memset(&fault_ctx, 0, sizeof(fault_ctx));
            fault_ctx.vpn = address >> 12;
            fault_ctx.page_table = page_table;
            fault_ctx.alloc_in.metadata_frames = max_level;
            return fault_ctx;
    }

private:
    // Helper functions
    void allocate_page_table_frames(FaultCtx& ctx,
                                    UInt64 address, UInt64 core_id, UInt64 ppn, int page_size, int num_requested_frames);
    int update_page_table_frames(UInt64 address, UInt64 core_id, UInt64 ppn, int page_size, std::vector<UInt64> &frames);
};