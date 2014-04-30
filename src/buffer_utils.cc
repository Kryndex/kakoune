#include "buffer_utils.hh"

#include "event_manager.hh"

namespace Kakoune
{

CharCount get_column(const Buffer& buffer,
                     CharCount tabstop, BufferCoord coord)
{
    auto& line = buffer[coord.line];
    auto col = 0_char;
    for (auto it = line.begin();
         it != line.end() and coord.column > (int)(it - line.begin());
         it = utf8::next(it))
    {
        if (*it == '\t')
            col = (col / tabstop + 1) * tabstop;
        else
            ++col;
    }
    return col;
}

Buffer* create_fifo_buffer(String name, int fd)
{
    Buffer* buffer = new Buffer(std::move(name), Buffer::Flags::Fifo | Buffer::Flags::NoUndo);

    auto watcher = new FDWatcher(fd, [buffer](FDWatcher& watcher) {
        constexpr size_t buffer_size = 1024 * 16;
        char data[buffer_size];
        ssize_t count = read(watcher.fd(), data, buffer_size);
        buffer->insert(buffer->end()-1, count > 0 ? String(data, data+count)
                                                  : "*** kak: fifo closed ***\n");
        if (count <= 0)
        {
            kak_assert(buffer->flags() & Buffer::Flags::Fifo);
            buffer->flags() &= ~Buffer::Flags::Fifo;
            buffer->flags() &= ~Buffer::Flags::NoUndo;
            close(watcher.fd());
            delete &watcher;
        }
    });

    buffer->hooks().add_hook("BufClose", "",
        [buffer, watcher](const String&, const Context&) {
            // Check if fifo is still alive, else watcher is already dead
            if (buffer->flags() & Buffer::Flags::Fifo)
            {
                close(watcher->fd());
                delete watcher;
            }
        });

    return buffer;
}

}