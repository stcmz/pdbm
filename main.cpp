#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <set>
#include <iomanip>
#include <boost/program_options.hpp>
#include "string.hpp"
using namespace std;
using namespace std::filesystem;
using namespace boost::program_options;

typedef array<double, 3> vector3;

struct atom
{
	bool valid;
	string name;
	vector3 coord;
};

typedef function<void(const atom&)> callback;

struct atom_iterator
{
	virtual void atoms(istream& istream, callback onatom) = 0;
};

struct measurer
{
	size_t atom_num = 0;
	vector3 lower_bounds{}, upper_bounds{};
	vector3 box_center{}, box_size{};
	vector3 geometric_center{};
	map<string, size_t> elem_num{};

	void measure(istream& istream, atom_iterator& it)
	{
		atom_num = 0;
		lower_bounds.fill(numeric_limits<double>::infinity());
		upper_bounds.fill(-numeric_limits<double>::infinity());
		box_center = {};
		geometric_center = {};
		box_size = {};
		elem_num.clear();

		it.atoms(istream, [&](const atom& a)
			{
				++atom_num;

				if (a.valid)
				{
					for (size_t i = 0; i < 3; ++i)
					{
						lower_bounds[i] = min(lower_bounds[i], a.coord[i]);
						upper_bounds[i] = max(upper_bounds[i], a.coord[i]);
						geometric_center[i] += a.coord[i];
					}

					++elem_num[a.name];
				}
			});

		for (size_t i = 0; i < 3; ++i)
		{
			box_center[i] = (upper_bounds[i] + lower_bounds[i]) / 2;
			box_size[i] = upper_bounds[i] - lower_bounds[i];
			geometric_center[i] /= atom_num;
		}
	}
};

struct pdb_atom_iterator : atom_iterator
{
	void atoms(istream& istream, callback onatom)
	{
		set<string> whitelist = { "ATOM", "HETATM" };

		for (string line; safe_getline(istream, line);)
		{
			if (line.size() < 78)
				continue;

			auto sig = trim_end(line.substr(0, 6));
			if (!whitelist.count(sig))
				continue;

			onatom(atom
				{
					true,
					trim_start(line.substr(76, 2)),
					{ stod(line.substr(30, 8)), stod(line.substr(38, 8)), stod(line.substr(46, 8)) },
				});
		}
	}
};

struct mol2_atom_iterator : atom_iterator
{
	void atoms(istream& istream, callback onatom)
	{
		bool inatoms = false;
		for (string line; safe_getline(istream, line);)
		{
			line = trim(line);
			if (line.length() == 0)
				continue;

			if (inatoms)
			{
				if (line[0] == '@')
					break;
			}
			else
			{
				if (line == "@<TRIPOS>ATOM")
					inatoms = true;
				continue;
			}

			auto fields = split(line, string(" \t\v\f"));
			if (fields.size() < 5)
			{
				// error
				onatom(atom{ false });
				continue;
			}

			onatom(atom
				{
					true,
					fields[1],
					{ stod(fields[2]), stod(fields[3]), stod(fields[4]) },
				});
		}
	}
};

int main(int argc, char* argv[])
{
	static double default_magnification = 2.0;
	static string default_input_format = "pdb";
	set<string> supported_formats = { "pdb", "pdbqt", "mol2" };

	try
	{
		bool all_dimensions, no_ligand_argument, use_centroid;
		path input_path;
		double magnification, lb_size, ub_size;
		string input_fmt;

		options_description options("Options");
		options.add_options()
			("input-file,i", value<path>(&input_path), (join("/", supported_formats.begin(), supported_formats.end()) + " file to be measured, without this argument user will be prompted to input lines").c_str())
			("input-format,f", value<string>(&input_fmt), ("supported input formats: " + join(", ", supported_formats.begin(), supported_formats.end())).c_str())
			("all,a", bool_switch(&all_dimensions), "output detailed dimensional information instead of the form of AutoDock Vina and idock arguments which is the default")
			("magnification,m", value<double>(&magnification)->default_value(default_magnification), "magnification ratio on the measured size (ignored if --all specified)")
			("lower-bound,l", value<double>(&lb_size), "limit the lower bound for the --size_* arguments in all three dimensions for Vina (ignored if --all specified)")
			("upper-bound,u", value<double>(&ub_size), "limit the upper bound for the --size_* arguments in all three dimensions for Vina (ignored if --all specified)")
			("no-ligand-argument,L", bool_switch(&no_ligand_argument), "do not emit --ligand argument for Vina (ignored if --all specified)")
			("centroid,c", bool_switch(&use_centroid), "use centroid (i.e. geometric center, center of mass, or center of gravity) instead of bounding box center when generating --center_* arguments for Vina (ignored if --all specified)")
			("help", "this help information")
			("version", "version information");

		positional_options_description positional;
		positional.add("input-file", 1);

		variables_map vm;
		store(command_line_parser(argc, argv).options(options).positional(positional).run(), vm);
		notify(vm);

		if (vm.count("help")) // program input
		{
			cout << "Usage: " << argv[0] << " [<input-file>] [options]" << endl;
			cout << join("/", supported_formats.begin(), supported_formats.end()) + " measuring tool by ryan@imozo.cn" << endl;
			cout << endl;
			cout << options;
			return 0;
		}

		if (vm.count("version"))
		{
			cout << "1.0.4 (2020-08-25)" << endl;
			return 0;
		}

		vector<string> lines;

		if (!vm.count("input-format") && vm.count("input-file"))
		{
			auto ext = input_path.extension().string();
			if (ext.size())
				ext = ext.substr(1);
			input_fmt = ext;
		}

		if (input_fmt.empty())
			input_fmt = default_input_format;

		if (!supported_formats.count(input_fmt))
		{
			cerr << "ERROR: unknown format '" << input_fmt << "', supported formats are " << join(", ", supported_formats.begin(), supported_formats.end()) << endl;
			return 1;
		}

		unique_ptr<atom_iterator> pit;
		if (starts_with(input_fmt, "pdb"))
			pit = make_unique<pdb_atom_iterator>();
		else
			pit = make_unique<mol2_atom_iterator>();

		measurer m;
		if (!vm.count("input-file"))
		{
			m.measure(cin, *pit);
		}
		else
		{
			ifstream file(input_path);
			m.measure(file, *pit);
		}

		if (all_dimensions)
		{
			if (vm.count("input-file"))
				cout << "file: " << input_path << endl;
			if (m.atom_num)
			{
				cout.precision(3);
				cout.setf(ios::fixed);

				cout << "atoms: " << m.atom_num << endl;
				for (auto [elem, num] : m.elem_num)
					cout << "  " << elem << " atoms: " << num << endl;
				cout << "x bounds: [" << m.lower_bounds[0] << ", " << m.upper_bounds[0] << "]" << endl;
				cout << "y bounds: [" << m.lower_bounds[1] << ", " << m.upper_bounds[1] << "]" << endl;
				cout << "z bounds: [" << m.lower_bounds[2] << ", " << m.upper_bounds[2] << "]" << endl;
				cout << "bounding box center: (" << m.box_center[0] << ", " << m.box_center[1] << ", " << m.box_center[2] << ")" << endl;
				cout << "bounding box size: (" << m.box_size[0] << ", " << m.box_size[1] << ", " << m.box_size[2] << ")" << endl;
				cout << "geometric center: (" << m.geometric_center[0] << ", " << m.geometric_center[1] << ", " << m.geometric_center[2] << ")" << endl;
			}
			else
			{
				cout << "atoms: 0" << endl;
			}
		}
		else
		{
			if (m.atom_num)
			{
				auto [size_x, size_y, size_z] = m.box_size;
				auto [center_x, center_y, center_z] = (use_centroid ? m.geometric_center : m.box_center);

				size_x *= magnification;
				size_y *= magnification;
				size_z *= magnification;

				if (vm.count("lower-bound"))
				{
					size_x = max(size_x, lb_size);
					size_y = max(size_y, lb_size);
					size_z = max(size_z, lb_size);
				}
				if (vm.count("upper-bound"))
				{
					size_x = min(size_x, ub_size);
					size_y = min(size_y, ub_size);
					size_z = min(size_z, ub_size);
				}

				if (!no_ligand_argument && vm.count("input-file"))
					cout << "--ligand=" << input_path << ' ';
				cout << "--center_x=" << center_x
					<< " --center_y=" << center_y
					<< " --center_z=" << center_z
					<< " --size_x=" << size_x
					<< " --size_y=" << size_y
					<< " --size_z=" << size_z
					<< endl;
			}
			else
			{
				cerr << "ERROR: no atom found for Vina arguments";
				if (vm.count("input-file"))
					cout << " in file " << input_path;
				cout << endl;
				return 2;
			}
		}

		return 0;
	}
	catch (exception& ex)
	{
		cerr << "ERROR: " << ex.what() << endl;
		return 3;
	}
}