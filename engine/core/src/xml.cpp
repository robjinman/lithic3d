#include "lithic3d/xml.hpp"
#include "lithic3d/exception.hpp"
#include "lithic3d/utils.hpp"
#include <tinyxml2.h>
#include <map>

using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;
using tinyxml2::XMLAttribute;
using tinyxml2::XMLError;

namespace lithic3d
{

XmlNode::Iterator::Iterator(std::vector<std::unique_ptr<XmlNode>>::const_iterator i)
  : m_i(i)
{
}

bool XmlNode::Iterator::operator==(const Iterator& rhs) const
{
  return m_i == rhs.m_i;
}

bool XmlNode::Iterator::operator!=(const Iterator& rhs) const
{
  return !(*this == rhs);
}

const XmlNode& XmlNode::Iterator::operator*() const
{
  return **m_i;
}

const XmlNode* XmlNode::Iterator::operator->() const
{
  return m_i->get();
}

XmlNode::Iterator& XmlNode::Iterator::operator++()
{
  next();
  return *this;
}

void XmlNode::Iterator::next()
{
  ++m_i;
}

namespace
{

class XmlNodeImpl : public XmlNode
{
  public:
    explicit XmlNodeImpl(const std::string& name);
    explicit XmlNodeImpl(const XmlNodeImpl& cpy);
    explicit XmlNodeImpl(const XMLElement& node);

    XmlNodePtr clone() const override;
    const std::string& name() const override;
    const std::string& value() const override;
    Iterator child(const std::string& name) const override;
    std::string attribute(const std::string& name) const override;
    Iterator begin() const override;
    Iterator end() const override;

    void addChild(XmlNodePtr node) override;
    void setValue(const std::string& value) override;
    void setAttribute(const std::string& name, const std::string& value) override;
    void write(std::ostream& stream) const override;

  private:
    std::string m_name;
    std::string m_value;
    std::map<std::string, std::string> m_attributes;
    std::vector<std::unique_ptr<XmlNode>> m_children;

    void write(std::ostream& stream, size_t level) const;
};

XmlNodeImpl::XmlNodeImpl(const std::string& name)
  : m_name(name)
{}

XmlNodeImpl::XmlNodeImpl(const XMLElement& node)
{
  m_name = node.Name();
  auto value = node.GetText();
  if (value != nullptr) {
    m_value = value;
  }

  for (auto attr = node.FirstAttribute(); attr != nullptr; attr = attr->Next()) {
    std::string attrName = attr->Name();
    std::string attrValue = attr->Value();
    m_attributes[attrName] = attrValue;
  }

  for (auto elem = node.FirstChildElement(); elem != nullptr; elem = elem->NextSiblingElement()) {
    std::string name = elem->Name();
    m_children.push_back(std::make_unique<XmlNodeImpl>(*elem));
  }
}

XmlNodeImpl::XmlNodeImpl(const XmlNodeImpl& cpy)
  : m_name(cpy.m_name)
  , m_value(cpy.m_value)
  , m_attributes(cpy.m_attributes)
{
  for (auto& child : cpy.m_children) {
    m_children.push_back(child->clone());
  }
}

XmlNodePtr XmlNodeImpl::clone() const
{
  return std::make_unique<XmlNodeImpl>(*this);
}

const std::string& XmlNodeImpl::name() const
{
  return m_name;
}

const std::string& XmlNodeImpl::value() const
{
  return m_value;
}

XmlNode::Iterator XmlNodeImpl::child(const std::string& name) const
{
  for (auto i = m_children.begin(); i != m_children.end(); ++i) {
    if ((*i)->name() == name) {
      return i;
    }
  }

  return end();
}

std::string XmlNodeImpl::attribute(const std::string& name) const
{
  auto i = m_attributes.find(name);
  return i != m_attributes.end() ? i->second : "";
}

XmlNode::Iterator XmlNodeImpl::begin() const
{
  return Iterator(m_children.begin());
}

XmlNode::Iterator XmlNodeImpl::end() const
{
  return Iterator(m_children.end());
}

void XmlNodeImpl::addChild(XmlNodePtr node)
{
  m_children.push_back(std::move(node));
}

void XmlNodeImpl::setValue(const std::string& value)
{
  m_value = value;
}

void XmlNodeImpl::setAttribute(const std::string& name, const std::string& value)
{
  m_attributes[name] = value;
}

void XmlNodeImpl::write(std::ostream& stream, size_t level) const
{
  auto tabSpaces = [&stream](size_t n) {
    const int tabSize = 2;
    stream << std::string(n * tabSize, ' ');
  };

  tabSpaces(level);
  stream << "<" << m_name;
  for (auto& [name, value] : m_attributes) {
    stream << " " << name << "=\"" << value << "\"";
  }
  stream << ">\n";

  if (!m_value.empty()) {
    tabSpaces(level + 1);
    stream << m_value << "\n";
  }

  for (auto& child : m_children) {
    auto& ref = dynamic_cast<const XmlNodeImpl&>(*child);
    ref.write(stream, level + 1);
  }

  tabSpaces(level);
  stream << "</" << m_name << ">\n";
}

void XmlNodeImpl::write(std::ostream& stream) const
{
  write(stream, 0);
}

} // namespace

XmlNodePtr createXmlNode(const std::string& name)
{
  return std::make_unique<XmlNodeImpl>(name);
}

XmlNodePtr parseXml(std::string_view data)
{
  XMLDocument doc;
  auto result = doc.Parse(data.data(), data.size());
  if (result != XMLError::XML_SUCCESS) {
    EXCEPTION("Error parsing XML");
  }

  return std::make_unique<XmlNodeImpl>(*doc.RootElement());
}

XmlNodePtr parseXml(const std::vector<char>& data)
{
  XMLDocument doc;
  auto result = doc.Parse(data.data(), data.size());
  if (result != XMLError::XML_SUCCESS) {
    EXCEPTION("Error parsing XML");
  }

  return std::make_unique<XmlNodeImpl>(*doc.RootElement());
}

} // namespace lithic3d
