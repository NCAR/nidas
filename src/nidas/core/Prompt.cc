
#include "Prompt.h"

#include <nidas/util/InvalidParameterException.h>
#include "XDOM.h"
#include <sstream>

namespace n_u = nidas::util;

namespace nidas { namespace core {


Prompt::Prompt(const std::string& promptString, double promptRate,
               double promptOffset):
    _promptString(promptString),
    _promptRate(promptRate),
    _promptOffset(promptOffset)
{}


void Prompt::setString(const std::string& val) {
    _promptString = val;
}

const std::string& Prompt::getString() const {
    return _promptString;
}

/**
 * Set rate of desired prompting, in Hz (sec^-1).
 */
void Prompt::setRate(const double val) {
    _promptRate = val;
}

double Prompt::getRate() const {
    return _promptRate;
}

/**
 * Set prompt offset in seconds.  For example, for a rate of 10Hz, an offset
 * of 0 would result in prompts at 0.0, 0.1, 0.2 seconds after each second.
 * An offset of 0.01 would result in prompts at 0.01, 0.11, 0.21 seconds after the second.
 */
void Prompt::setOffset(const double val) {
    _promptOffset = val;
}

double Prompt::getOffset() const {
    return _promptOffset;
}

bool Prompt::valid() const
{
    return (_promptRate > 0.0) && _promptString.size();
}

bool Prompt::hasPrompt() const
{
    return _promptString.size();
}

void
Prompt::fromDOMElement(const xercesc::DOMElement* node)
{
    // reset to defaults, so this prompt will be only what is set in the xml.
    *this = Prompt();
    XDOMElement xchild((xercesc::DOMElement*) node);

    if (node->hasAttribute(XMLStringConverter("string")))
    {
        std::string prompt = xchild.getAttributeValue("string");
        setString(prompt);
    }
    if (node->hasAttribute(XMLStringConverter("rate")))
    {
        std::istringstream ist(xchild.getAttributeValue("rate"));
        double promptrate;
        ist >> promptrate;
        if (ist.fail() || promptrate < 0.0) {
            std::ostringstream ost;
            ost << "prompt rate '" << xchild.getAttributeValue("rate") << "'"
                << " must be float >= 0";
            throw n_u::InvalidParameterException(ost.str());
        }
        setRate(promptrate);
    }
    if (node->hasAttribute(XMLStringConverter("offset")))
    {
        std::string offsetStr = xchild.getAttributeValue("offset");
        std::istringstream ist(offsetStr);
        double offset;
        ist >> offset;
        if (ist.fail()) {
            std::ostringstream ost;
            ost << "invalid prompt offset '" << offsetStr << "'";
            throw n_u::InvalidParameterException(ost.str());
        }
        setOffset(offset);
    }
}

bool
Prompt::operator==(const Prompt& right) const
{
    return (_promptString == right._promptString) &&
           (_promptRate == right._promptRate) &&
           (_promptOffset == right._promptOffset);
}

std::string
Prompt::toXML() const
{
    std::ostringstream out;
    out << "<prompt";
    if (_promptString.size())
        out << " string='" << _promptString << "'";
    if (_promptRate)
        out << " rate='" << _promptRate << "'";
    if (_promptOffset)
        out << " offset='" << _promptOffset << "'";
    out << "/>";
    return out.str();
}

std::ostream&
operator<<(std::ostream& out, const Prompt& prompt)
{
    out << prompt.toXML();
    return out;
}

}} // namespace core, namespace nidas
