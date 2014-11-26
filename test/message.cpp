#include <gtest/gtest.h>

#include "../message.hpp"

using cerb::rint;
using cerb::byte;
using cerb::msg::MessageInterrupted;

namespace {

    typedef std::string::iterator InputIterator;

    class SimpleMessageCollector {
    public:
        rint last_int;
        std::string last_string;
        std::string last_simple_string;
        std::string last_error;

        SimpleMessageCollector()
            : last_int(0)
        {}

        InputIterator on_int(rint val, InputIterator next)
        {
            this->last_int = val;
            return next;
        }

        static std::pair<InputIterator, std::string>
            get_simple_string(InputIterator begin, InputIterator end)
        {
            std::string r;
            while (begin != end) {
                byte b = *begin++;
                if (b == '\r') {
                    if (begin != end) {
                        ++begin;
                    }
                    return std::make_pair(begin, r);
                }
                r += char(b);
            }
            return std::make_pair(end, r);
        }

        InputIterator on_sstr(InputIterator begin, InputIterator end)
        {
            auto r = get_simple_string(begin, end);
            this->last_simple_string = r.second;
            return r.first;
        }

        InputIterator on_str(rint size, InputIterator begin, InputIterator)
        {
            last_string = std::string(begin, begin + size);
            return begin + size + 2;
        }

        void on_arr(rint)
        {
            EXPECT_TRUE(false) << "unexpected call on_arr";
        }

        InputIterator on_nil(InputIterator n)
        {
            EXPECT_TRUE(false) << "unexpected call on_nil";
            return n;
        }

        InputIterator on_err(InputIterator begin, InputIterator end)
        {
            auto r = get_simple_string(begin, end);
            this->last_error = r.second;
            return r.first;
        }
    };

}

TEST(Message, SimpleElement)
{
    std::string t(":1234\r\n");
    SimpleMessageCollector c;
    auto r = cerb::msg::parse(t.begin(), t.end(), c);
    ASSERT_EQ(1234, c.last_int);
    ASSERT_EQ(t.end(), r);

    t = ":-1234\r\n+PONG\r\n$14\r\nEl Psy Congroo\r\n-ERR ASK\r\n";
    r = cerb::msg::parse(t.begin(), t.end(), c);
    ASSERT_EQ(-1234, c.last_int);
    ASSERT_EQ('+', *r);

    r = cerb::msg::parse(r, t.end(), c);
    ASSERT_EQ("PONG", c.last_simple_string);
    ASSERT_EQ('$', *r);

    r = cerb::msg::parse(r, t.end(), c);
    ASSERT_EQ("El Psy Congroo", c.last_string);
    ASSERT_EQ('-', *r);

    r = cerb::msg::parse(r, t.end(), c);
    ASSERT_EQ("ERR ASK", c.last_error);
    ASSERT_EQ(t.end(), r);
}

TEST(Message, SplitSimpleMessage)
{
    SimpleMessageCollector c;

    std::string t(":1234\r\n");
    auto bms_a(cerb::msg::split(t.begin(), t.end()));

    auto i = bms_a.begin();
    ASSERT_NE(bms_a.end(), i);
    cerb::msg::parse(i.range_begin(), i.range_end(), c);
    ASSERT_TRUE(bms_a.finished());
    ASSERT_EQ(1234, c.last_int);
    ++i;

    ASSERT_EQ(i, bms_a.end());

    t = ":-5678\r\n+PONG\r\n$14\r\nEl Psy Congroo\r\n-ERR ASK\r\n";
    auto bms_b(cerb::msg::split(t.begin(), t.end()));
    ASSERT_TRUE(bms_b.finished());

    i = bms_b.begin();
    ASSERT_NE(bms_b.end(), i);
    cerb::msg::parse(i.range_begin(), i.range_end(), c);
    ASSERT_EQ(-5678, c.last_int);
    ++i;

    ASSERT_NE(bms_b.end(), i);
    cerb::msg::parse(i.range_begin(), i.range_end(), c);
    ASSERT_EQ("PONG", c.last_simple_string);
    ++i;

    ASSERT_NE(bms_b.end(), i);
    cerb::msg::parse(i.range_begin(), i.range_end(), c);
    ASSERT_EQ("El Psy Congroo", c.last_string);
    ++i;

    ASSERT_NE(bms_b.end(), i);
    cerb::msg::parse(i.range_begin(), i.range_end(), c);
    ASSERT_EQ("ERR ASK", c.last_error);
    ++i;

    ASSERT_EQ(i, bms_b.end());
}

TEST(Message, SplitMessageWithArray)
{
    SimpleMessageCollector c;

    std::string t("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n"
                  "*3\r\n:7\r\n:8\r\n:9\r\n");
    auto bms_a(cerb::msg::split(t.begin(), t.end()));
    ASSERT_TRUE(bms_a.finished());

    auto i = bms_a.begin();
    ASSERT_NE(bms_a.end(), i);
    ASSERT_EQ("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n",
              std::string(i.range_begin(), i.range_end()));
    ++i;

    ASSERT_NE(bms_a.end(), i);
    ASSERT_EQ("*3\r\n:7\r\n:8\r\n:9\r\n",
              std::string(i.range_begin(), i.range_end()));
    ++i;

    ASSERT_EQ(i, bms_a.end());

    std::string x("*2\r\n"
                      "*3\r\n"
                          ":1\r\n"
                          ":2\r\n"
                          ":3\r\n"
                      "*2\r\n"
                          "+Foo\r\n"
                          "-Bar\r\n");
    t = x + "$-1\r\n*-1\r\n";
    auto bms_b(cerb::msg::split(t.begin(), t.end()));
    ASSERT_TRUE(bms_b.finished());

    i = bms_b.begin();
    ASSERT_NE(bms_b.end(), i);
    ASSERT_EQ(x, std::string(i.range_begin(), i.range_end()));
    ++i;

    ASSERT_NE(bms_b.end(), i);
    ASSERT_EQ("$-1\r\n", std::string(i.range_begin(), i.range_end()));
    ++i;

    ASSERT_NE(bms_b.end(), i);
    ASSERT_EQ("*-1\r\n", std::string(i.range_begin(), i.range_end()));
    ++i;

    ASSERT_EQ(i, bms_b.end());
}

TEST(Message, InterruptedMessage)
{
    SimpleMessageCollector c;
    {
        std::string t("+OK\r");
        auto bms(cerb::msg::split(t.begin(), t.end()));
        ASSERT_FALSE(bms.finished());
        ASSERT_EQ(0, bms.size());
        ASSERT_EQ(t.begin(), bms.interrupt_point());
    }
    {
        std::string f("+PONG\r\n");
        std::string t(f + ":");
        auto bms(cerb::msg::split(t.begin(), t.end()));
        ASSERT_FALSE(bms.finished());
        ASSERT_EQ(1, bms.size());
        ASSERT_EQ(t.begin() + f.size(), bms.interrupt_point());
    }
    {
        std::string f("+PONG\r\n");
        std::string t(f + "*2\r\n$3\r\nfoo\r\n");
        auto bms(cerb::msg::split(t.begin(), t.end()));
        ASSERT_FALSE(bms.finished());
        ASSERT_EQ(1, bms.size());
        ASSERT_EQ(t.begin() + f.size(), bms.interrupt_point());
    }
    {
        std::string f(":123\r\n");
        std::string t(f + ":-");
        auto bms(cerb::msg::split(t.begin(), t.end()));
        ASSERT_FALSE(bms.finished());
        ASSERT_EQ(1, bms.size());
        ASSERT_EQ(t.begin() + f.size(), bms.interrupt_point());
    }
}
