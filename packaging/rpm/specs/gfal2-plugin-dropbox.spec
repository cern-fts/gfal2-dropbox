# unversionned doc dir F20 change https://fedoraproject.org/wiki/Changes/UnversionedDocdirs
%{!?_pkgdocdir: %global _pkgdocdir %{_docdir}/%{name}-%{version}}

Name:           gfal2-plugin-dropbox
Version:        0.0.1
Release:        1%{?dist}
Summary:        Provide Dropbox support for GFAL2

Group:          Applications/Internet
License:        ASL 2.0
URL:            http://dmc.web.cern.ch/projects/gfal-2/home
Source0:        %{name}/%{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  cmake
BuildRequires:  gfal2-devel
BuildRequires:  json-c-devel
%if %{?fedora}%{!?fedora:0} >= 10 || %{?rhel}%{!?rhel:0} >= 6
BuildRequires:  libcurl-devel
%else
BuildRequires:  curl-devel
%endif
BuildRequires:  openssl-devel

%description
The Grid File Access Library, GFAL2, provides a simple POSIX-like API for file
operations in grid and cloud environments. Plug-ins are available to allow
access via a variety of protocols. This package contains a plugin for the
Dropbox API protocol (dropbox://).

%global pkgdir gfal2-plugins

%prep
%setup -q

%build
%cmake \
-DCMAKE_INSTALL_PREFIX=/ \
-DDOC_INSTALL_DIR=%{_pkgdocdir} \
 . 

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%clean
rm -rf %{buildroot}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%config(noreplace) %{_sysconfdir}/gfal2.d/dropbox_plugin.conf
%{_libdir}/%{pkgdir}/libgfal_plugin_dropbox.so
%{_pkgdocdir}/*

%changelog
* Fri Jul 18 2014 Alejandro Alvarez Ayllon <aalvarez at cern.ch> - 0.0.1-1
 - New release
