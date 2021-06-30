#pragma once

#include "./handle.hpp"

namespace oomph
{
class region
{
  public:
    using handle_type = handle;

  private:
    MPI_Comm m_comm;
    MPI_Win  m_win;
    void*    m_ptr;

  public:
    region(MPI_Comm comm, MPI_Win win, void* ptr, std::size_t size)
    : m_comm{comm}
    , m_win{win}
    , m_ptr{ptr}
    {
        OOMPH_CHECK_MPI_RESULT(MPI_Win_attach(m_win, ptr, size));
    }

    region(region const&) = delete;

    region(region&& r) noexcept
    : m_comm{r.m_comm}
    , m_win{r.m_win}
    , m_ptr{std::exchange(r.m_ptr, nullptr)}
    {
    }

    ~region()
    {
        if (m_ptr) MPI_Win_detach(m_win, m_ptr);
    }

    // get a handle to some portion of the region
    handle_type get_handle(std::size_t offset, std::size_t size)
    {
        return {(void*)((char*)m_ptr + offset), size};
    }
};
} // namespace oomph