using System;
using System.Collections;
using System.ComponentModel;

namespace Flobbster.Windows.Forms
{
    public delegate void PropertySpecEventHandler(object sender, PropertySpecEventArgs e);

    public class PropertySpec
    {
        public PropertySpec(string name, Type type, string category, string description)
            : this(name, type, category, description, null, null, null)
        {
        }

        public PropertySpec(string name, Type type, string category, string description, object defaultValue)
            : this(name, type, category, description, defaultValue, null, null)
        {
        }

        public PropertySpec(string name, Type type, string category, string description, object defaultValue, object editor, object converter)
        {
            Name = name;
            Type = type;
            Category = category;
            Description = description;
            DefaultValue = defaultValue;
            Editor = editor;
            Converter = converter;
            Attributes = Array.Empty<Attribute>();
        }

        public string Name { get; set; }
        public Type Type { get; set; }
        public string Category { get; set; }
        public string Description { get; set; }
        public object DefaultValue { get; set; }
        public object Editor { get; set; }
        public object Converter { get; set; }
        public Attribute[] Attributes { get; set; }
    }

    public class PropertySpecEventArgs : EventArgs
    {
        public PropertySpecEventArgs(PropertySpec property, object value)
        {
            Property = property;
            Value = value;
        }

        public PropertySpec Property { get; }
        public object Value { get; set; }
    }

    public class PropertySpecCollection : CollectionBase
    {
        public PropertySpec this[int index] => (PropertySpec)List[index];
        public void Add(PropertySpec item) => List.Add(item);
        public void Remove(PropertySpec item) => List.Remove(item);
        public bool Contains(PropertySpec item) => List.Contains(item);
    }

    public class PropertyBag : Component, ICustomTypeDescriptor
    {
        public class PropertySpecDescriptor : PropertyDescriptor
        {
            public PropertySpecDescriptor(PropertyBag bag, PropertySpec item)
                : base(item?.Name ?? string.Empty, item?.Attributes ?? Array.Empty<Attribute>())
            {
                this.bag = bag;
                this.item = item;
            }

            public readonly PropertyBag bag;
            public readonly PropertySpec item;

            public override bool CanResetValue(object component) => true;
            public override Type ComponentType => bag?.GetType() ?? typeof(PropertyBag);
            public override object GetValue(object component) => bag?.ReadValue(item);
            public override bool IsReadOnly => false;
            public override Type PropertyType => item?.Type ?? typeof(object);
            public override void ResetValue(object component) => SetValue(component, item?.DefaultValue);

            public override void SetValue(object component, object value)
            {
                bag?.WriteValue(item, value);
                OnValueChanged(component, EventArgs.Empty);
            }

            public override bool ShouldSerializeValue(object component) => true;
            public override string Category => item?.Category ?? base.Category;
            public override string Description => item?.Description ?? base.Description;

            public override TypeConverter Converter
            {
                get
                {
                    if (item?.Converter is Type converterType && typeof(TypeConverter).IsAssignableFrom(converterType))
                        return (TypeConverter)Activator.CreateInstance(converterType);
                    return base.Converter;
                }
            }

            public override object GetEditor(Type editorBaseType)
            {
                if (item?.Editor is Type editorType && editorBaseType.IsAssignableFrom(editorType))
                    return Activator.CreateInstance(editorType);
                return base.GetEditor(editorBaseType);
            }
        }

        public event PropertySpecEventHandler GetValue;
        public event PropertySpecEventHandler SetValue;

        public PropertySpecCollection Properties { get; } = new PropertySpecCollection();

        internal object ReadValue(PropertySpec property)
        {
            var args = new PropertySpecEventArgs(property, property?.DefaultValue);
            GetValue?.Invoke(this, args);
            return args.Value;
        }

        internal void WriteValue(PropertySpec property, object value)
        {
            var args = new PropertySpecEventArgs(property, value);
            SetValue?.Invoke(this, args);
        }

        AttributeCollection ICustomTypeDescriptor.GetAttributes() => TypeDescriptor.GetAttributes(this, true);
        string ICustomTypeDescriptor.GetClassName() => TypeDescriptor.GetClassName(this, true);
        string ICustomTypeDescriptor.GetComponentName() => TypeDescriptor.GetComponentName(this, true);
        TypeConverter ICustomTypeDescriptor.GetConverter() => TypeDescriptor.GetConverter(this, true);
        EventDescriptor ICustomTypeDescriptor.GetDefaultEvent() => TypeDescriptor.GetDefaultEvent(this, true);
        PropertyDescriptor ICustomTypeDescriptor.GetDefaultProperty() => null;
        object ICustomTypeDescriptor.GetEditor(Type editorBaseType) => TypeDescriptor.GetEditor(this, editorBaseType, true);
        EventDescriptorCollection ICustomTypeDescriptor.GetEvents(Attribute[] attributes) => TypeDescriptor.GetEvents(this, attributes, true);
        EventDescriptorCollection ICustomTypeDescriptor.GetEvents() => TypeDescriptor.GetEvents(this, true);

        PropertyDescriptorCollection ICustomTypeDescriptor.GetProperties(Attribute[] attributes)
        {
            var descriptors = new ArrayList();
            foreach (PropertySpec property in Properties)
                descriptors.Add(new PropertySpecDescriptor(this, property));
            return new PropertyDescriptorCollection((PropertyDescriptor[])descriptors.ToArray(typeof(PropertyDescriptor)));
        }

        PropertyDescriptorCollection ICustomTypeDescriptor.GetProperties() => ((ICustomTypeDescriptor)this).GetProperties(null);
        object ICustomTypeDescriptor.GetPropertyOwner(PropertyDescriptor pd) => this;
    }
}
