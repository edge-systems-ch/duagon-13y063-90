%global debug_package %{nil}
%global _build_id_links none
%global _buildhost ci.edge.systems

Name:           duagon-13y063-90
Version:        0.0.0
Release:        1%{?dist}
Summary:        Linux wireless interface configuration tool for duagon F229, G239 and ME10

License:        Proprietary
Vendor:         duagon Germany GmbH
Packager:       Edge Systems GmbH
URL:            https://www.duagon.com/software/13y063-90/
Source0:        %{name}-%{version}.tar.gz

%description
13Y063-90 is a Linux wireless interface configuration tool for duagon F229,
G239 and ME10 devices.

%prep
%autosetup -n %{name}-%{version}

%build
cd linux
make

%install
rm -rf %{buildroot}

install -D -m 0755 linux/bin/13Y063-90 \
  %{buildroot}%{_bindir}/13Y063-90

%files
%doc README.md ReleaseNotes_13Y063-90.md
%license license.txt
%{_bindir}/13Y063-90