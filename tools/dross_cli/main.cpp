#include <dross/foundation/byte_codec.hpp>
#include <dross/foundation/quantities.hpp>
#include <dross/foundation/version.hpp>
#include <dross/identity/content_id.hpp>

#include <iostream>
#include <string_view>

namespace {

constexpr int usage_error = 2;
constexpr int validation_error = 3;
constexpr int self_check_error = 4;

void print_usage(std::ostream& output) {
  output << "usage:\n"
            "  dross_headless version\n"
            "  dross_headless validate-id <namespace:name>\n";
}

[[nodiscard]] bool foundation_self_check() {
  const auto content_id = dross::ContentId::parse("dross:foundation");
  if (!content_id) {
    return false;
  }

  dross::ByteWriter writer;
  writer.write(*content_id);
  dross::ByteReader reader{writer.bytes()};
  const auto decoded = reader.read_content_id();
  const auto next_tick = dross::Tick{0}.checked_add(1);
  return decoded && *decoded == *content_id && reader.remaining() == 0 && next_tick &&
         next_tick->value() == 1;
}

[[nodiscard]] std::string_view reason_text(const dross::ContentIdErrorReason reason) {
  switch (reason) {
  case dross::ContentIdErrorReason::missing_separator:
    return "missing ':' separator";
  case dross::ContentIdErrorReason::extra_separator:
    return "extra ':' separator";
  case dross::ContentIdErrorReason::empty_namespace:
    return "namespace is empty";
  case dross::ContentIdErrorReason::empty_name:
    return "name is empty";
  case dross::ContentIdErrorReason::invalid_character:
    return "invalid character";
  }
  return "unknown error";
}

} // namespace

int main(const int argument_count, const char* const arguments[]) {
  if (argument_count == 2 && std::string_view{arguments[1]} == "version") {
    if (!foundation_self_check()) {
      std::cerr << "foundation self-check failed\n";
      return self_check_error;
    }
    std::cout << dross::build_information() << '\n';
    return 0;
  }

  if (argument_count == 3 && std::string_view{arguments[1]} == "validate-id") {
    const std::string_view input{arguments[2]};
    const auto parsed = dross::ContentId::parse(input);
    if (!parsed) {
      std::cerr << "invalid ContentId at byte " << parsed.error().position << ": "
                << reason_text(parsed.error().reason) << '\n';
      return validation_error;
    }
    std::cout << parsed->canonical() << '\n';
    return 0;
  }

  print_usage(std::cerr);
  return usage_error;
}
