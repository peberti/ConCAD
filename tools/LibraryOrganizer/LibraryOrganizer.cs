/*
 * ConCAD Library Organizer
 *
 * A small stand-alone tool for ConCAD's SQLite libraries (.TCLib symbol
 * libraries and .CCMod module libraries): move or copy symbols and modules
 * between libraries, delete them, and edit their properties (name,
 * reference, description, fields). The drawings themselves are copied as
 * they are and never changed.
 *
 * Written for the C# 5 compiler that ships with the .NET Framework, so
 * build.bat works on any Windows PC without Visual Studio. SQLite comes
 * from winsqlite3.dll, which is part of Windows 10 and 11.
 *
 * License: LGPL 2.1, like ConCAD.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using Microsoft.Win32;

[assembly: System.Reflection.AssemblyTitle("ConCAD Library Organizer")]
[assembly: System.Reflection.AssemblyProduct("ConCAD")]

namespace ConCadLibraryOrganizer
{
	// ------------------------------------------------------------------
	// SQLite (winsqlite3.dll, part of Windows)

	static class Native
	{
		const string Dll = "winsqlite3.dll";
		public static readonly IntPtr Transient = new IntPtr(-1);

		[DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
		public static extern int sqlite3_open16(string filename, out IntPtr db);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_close(IntPtr db);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_busy_timeout(IntPtr db, int ms);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern IntPtr sqlite3_errmsg16(IntPtr db);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
		public static extern int sqlite3_prepare16_v2(IntPtr db, string sql, int nByte, out IntPtr stmt, IntPtr tail);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_step(IntPtr stmt);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_finalize(IntPtr stmt);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_bind_null(IntPtr stmt, int index);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_bind_int64(IntPtr stmt, int index, long value);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_bind_double(IntPtr stmt, int index, double value);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall, CharSet = CharSet.Unicode)]
		public static extern int sqlite3_bind_text16(IntPtr stmt, int index, string value, int nBytes, IntPtr destructor);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_bind_blob(IntPtr stmt, int index, byte[] value, int nBytes, IntPtr destructor);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_column_count(IntPtr stmt);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_column_type(IntPtr stmt, int col);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern long sqlite3_column_int64(IntPtr stmt, int col);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern double sqlite3_column_double(IntPtr stmt, int col);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern IntPtr sqlite3_column_text16(IntPtr stmt, int col);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern IntPtr sqlite3_column_blob(IntPtr stmt, int col);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern int sqlite3_column_bytes(IntPtr stmt, int col);
		[DllImport(Dll, CallingConvention = CallingConvention.StdCall)]
		public static extern long sqlite3_last_insert_rowid(IntPtr db);
	}

	sealed class Db : IDisposable
	{
		IntPtr handle;

		public Db(string path)
		{
			if (Native.sqlite3_open16(path, out handle) != 0)
			{
				string msg = Error();
				Dispose();
				throw new Exception("Cannot open " + path + ".\n" + msg);
			}
			Native.sqlite3_busy_timeout(handle, 5000);
		}

		string Error()
		{
			return handle == IntPtr.Zero ? "" : Marshal.PtrToStringUni(Native.sqlite3_errmsg16(handle));
		}

		public List<object[]> Query(string sql, params object[] args)
		{
			IntPtr stmt;
			if (Native.sqlite3_prepare16_v2(handle, sql, -1, out stmt, IntPtr.Zero) != 0)
			{
				throw new Exception(Error());
			}
			List<object[]> rows = new List<object[]>();
			try
			{
				for (int i = 0; i < args.Length; i++)
				{
					Bind(stmt, i + 1, args[i]);
				}
				while (true)
				{
					int rc = Native.sqlite3_step(stmt);
					if (rc == 101) // SQLITE_DONE
					{
						break;
					}
					if (rc != 100) // SQLITE_ROW
					{
						throw new Exception(Error());
					}
					int n = Native.sqlite3_column_count(stmt);
					object[] row = new object[n];
					for (int c = 0; c < n; c++)
					{
						switch (Native.sqlite3_column_type(stmt, c))
						{
							case 1:
								row[c] = Native.sqlite3_column_int64(stmt, c);
								break;
							case 2:
								row[c] = Native.sqlite3_column_double(stmt, c);
								break;
							case 3:
								row[c] = Marshal.PtrToStringUni(Native.sqlite3_column_text16(stmt, c));
								break;
							case 4:
								IntPtr p = Native.sqlite3_column_blob(stmt, c);
								byte[] b = new byte[Native.sqlite3_column_bytes(stmt, c)];
								if (b.Length > 0)
								{
									Marshal.Copy(p, b, 0, b.Length);
								}
								row[c] = b;
								break;
						}
					}
					rows.Add(row);
				}
			}
			finally
			{
				Native.sqlite3_finalize(stmt);
			}
			return rows;
		}

		void Bind(IntPtr stmt, int index, object value)
		{
			int rc;
			if (value == null)
			{
				rc = Native.sqlite3_bind_null(stmt, index);
			}
			else if (value is string)
			{
				rc = Native.sqlite3_bind_text16(stmt, index, (string) value, -1, Native.Transient);
			}
			else if (value is byte[])
			{
				byte[] b = (byte[]) value;
				rc = Native.sqlite3_bind_blob(stmt, index, b, b.Length, Native.Transient);
			}
			else if (value is double)
			{
				rc = Native.sqlite3_bind_double(stmt, index, (double) value);
			}
			else if (value is bool)
			{
				rc = Native.sqlite3_bind_int64(stmt, index, (bool) value ? 1 : 0);
			}
			else
			{
				rc = Native.sqlite3_bind_int64(stmt, index, Convert.ToInt64(value));
			}
			if (rc != 0)
			{
				throw new Exception(Error());
			}
		}

		public void Exec(string sql, params object[] args)
		{
			Query(sql, args);
		}

		public long LastId
		{
			get { return Native.sqlite3_last_insert_rowid(handle); }
		}

		// Runs work in one transaction; nothing is changed if it throws
		public void Transaction(Action work)
		{
			Exec("BEGIN TRANSACTION");
			try
			{
				work();
				Exec("COMMIT TRANSACTION");
			}
			catch
			{
				try { Exec("ROLLBACK TRANSACTION"); } catch { }
				throw;
			}
		}

		public void Dispose()
		{
			if (handle != IntPtr.Zero)
			{
				Native.sqlite3_close(handle);
				handle = IntPtr.Zero;
			}
		}

		public static string S(object o)
		{
			return o == null ? "" : Convert.ToString(o);
		}

		public static long L(object o)
		{
			try { return o == null ? 0 : Convert.ToInt64(o); } catch { return 0; }
		}
	}

	// ------------------------------------------------------------------
	// Library model: the tables ConCAD uses (LibrarySQLite.cpp)
	//   Name      one row per symbol name (Type 0 = symbol, 1 = module)
	//   Symbol    the drawing (XML), shared by the names of a symbol
	//   Attribute the fields of a name

	sealed class Field
	{
		public string Name = "";
		public string Value = "";
		public int Show;   // 0 shown, 1 hidden, 2 never shown
	}

	sealed class Entry
	{
		public long NameId;
		public long SymbolId;
		public int Type;
		public string Name = "";
		public string Reference = "";
		public string Description = "";
		public bool Connector;
		public List<Field> Fields = new List<Field>();

		public bool IsModule
		{
			get { return Type == 1; }
		}

		public Field Find(string name)
		{
			return Fields.FirstOrDefault(f => string.Equals(f.Name, name, StringComparison.OrdinalIgnoreCase));
		}

		public string FieldValue(string name)
		{
			Field f = Find(name);
			return f == null ? "" : f.Value;
		}

		// As CDlgUpdateBox::GetModuleName / GetModuleType in ConCAD
		public string ModuleName
		{
			get
			{
				Field f = Find("Name");
				if (f != null)
				{
					return f.Value;
				}
				return Description.Length == 0 ? Name : Description;
			}
		}

		public string ModuleType
		{
			get
			{
				Field f = Find("Type");
				if (f != null)
				{
					return f.Value;
				}
				return Find("Name") != null || Description.Length == 0 ? "" : Name;
			}
		}

		// The name ConCAD shows in its library window
		public string DisplayName
		{
			get
			{
				if (!IsModule)
				{
					return Name;
				}
				string t = ModuleType.Trim();
				string n = ModuleName.Trim();
				return t.Length == 0 ? n : t + " - " + n;
			}
		}

		// Fields ConCAD no longer shows (SPICE leftovers, the default Package)
		public static bool IsObsolete(Field f)
		{
			return f.Name.StartsWith("$$")
				|| (string.Equals(f.Name, "Package", StringComparison.OrdinalIgnoreCase) && f.Value == "package name for PCB layout");
		}
	}

	sealed class Library
	{
		public readonly string Path;
		static readonly HashSet<string> backedUp = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

		public Library(string path)
		{
			Path = path;
		}

		public bool ForModules
		{
			get { return Path.EndsWith(".CCMod", StringComparison.OrdinalIgnoreCase); }
		}

		public string Title
		{
			get { return System.IO.Path.GetFileNameWithoutExtension(Path); }
		}

		public string Kind
		{
			get { return ForModules ? "modules" : "symbols"; }
		}

		public override string ToString()
		{
			return Title + "   (" + Kind + ")";
		}

		static bool HasConnectorColumn(Db db)
		{
			foreach (object[] r in db.Query("PRAGMA table_info([Name])"))
			{
				if (string.Equals(Db.S(r[1]), "IsConnector", StringComparison.OrdinalIgnoreCase))
				{
					return true;
				}
			}
			return false;
		}

		static void EnsureConnectorColumn(Db db)
		{
			if (!HasConnectorColumn(db))
			{
				db.Exec("ALTER TABLE [Name] ADD COLUMN [IsConnector] INTEGER DEFAULT 0");
			}
		}

		// The first change to a library in a session keeps the old file as
		// <library>.bak
		void Backup()
		{
			if (backedUp.Add(Path) && File.Exists(Path))
			{
				File.Copy(Path, Path + ".bak", true);
			}
		}

		public string BackupPath
		{
			get { return Path + ".bak"; }
		}

		public List<Entry> Load()
		{
			if (!File.Exists(Path))
			{
				throw new Exception("The file does not exist:\n" + Path);
			}
			using (Db db = new Db(Path))
			{
				bool connector = HasConnectorColumn(db);
				List<Entry> list = new List<Entry>();
				Dictionary<long, Entry> byId = new Dictionary<long, Entry>();
				foreach (object[] r in db.Query("SELECT [NameID],[Name],[SymbolID],[Type],[Reference],[Description]"
					+ (connector ? ",[IsConnector]" : ",0") + " FROM [Name] WHERE [Type]=0 OR [Type]=1"))
				{
					Entry e = new Entry();
					e.NameId = Db.L(r[0]);
					e.Name = Db.S(r[1]);
					e.SymbolId = Db.L(r[2]);
					e.Type = (int) Db.L(r[3]);
					e.Reference = Db.S(r[4]);
					e.Description = Db.S(r[5]);
					e.Connector = Db.L(r[6]) != 0;
					list.Add(e);
					byId[e.NameId] = e;
				}
				foreach (object[] r in db.Query("SELECT [NameID],[AttName],[AttValue],[ShowAtt] FROM [Attribute] ORDER BY [AttributeID]"))
				{
					Entry e;
					if (byId.TryGetValue(Db.L(r[0]), out e))
					{
						Field f = new Field();
						f.Name = Db.S(r[1]);
						f.Value = Db.S(r[2]);
						f.Show = (int) Db.L(r[3]);
						e.Fields.Add(f);
					}
				}
				list.Sort((a, b) => string.Compare(a.DisplayName, b.DisplayName, StringComparison.CurrentCultureIgnoreCase));
				return list;
			}
		}

		// Writes the name, reference, description, connector flag and fields
		// of e (the drawing is not touched)
		public void Save(Entry e)
		{
			Backup();
			using (Db db = new Db(Path))
			{
				db.Transaction(delegate
				{
					bool connector = HasConnectorColumn(db);
					if (!connector && e.Connector)
					{
						EnsureConnectorColumn(db);
						connector = true;
					}
					if (connector)
					{
						db.Exec("UPDATE [Name] SET [Name]=?,[Reference]=?,[Description]=?,[IsConnector]=? WHERE [NameID]=?",
							e.Name, e.Reference, e.Description, e.Connector ? 1 : 0, e.NameId);
					}
					else
					{
						db.Exec("UPDATE [Name] SET [Name]=?,[Reference]=?,[Description]=? WHERE [NameID]=?",
							e.Name, e.Reference, e.Description, e.NameId);
					}
					db.Exec("DELETE FROM [Attribute] WHERE [NameID]=?", e.NameId);
					foreach (Field f in e.Fields)
					{
						db.Exec("INSERT INTO [Attribute] ([NameID],[AttName],[AttValue],[ShowAtt]) VALUES (?,?,?,?)",
							e.NameId, f.Name, f.Value, f.Show);
					}
				});
			}
		}

		// Copies a symbol or module (drawing, all its names and fields) from
		// another library into this one; returns the new name ids
		public List<long> CopyFrom(Library source, long symbolId)
		{
			byte[] data;
			List<object[]> names;
			Dictionary<long, List<object[]>> fields = new Dictionary<long, List<object[]>>();
			using (Db db = new Db(source.Path))
			{
				List<object[]> d = db.Query("SELECT CAST([Data] AS BLOB) FROM [Symbol] WHERE [SymbolID]=?", symbolId);
				if (d.Count == 0)
				{
					throw new Exception("The drawing of this item is missing in " + source.Title + ".");
				}
				data = d[0][0] as byte[] ?? new byte[0];
				names = db.Query("SELECT [NameID],[Name],[Type],[Reference],[ppp],[Description],[ShowName],[ShowRef],[DefRotate]"
					+ (HasConnectorColumn(db) ? ",[IsConnector]" : ",0")
					+ " FROM [Name] WHERE [SymbolID]=? ORDER BY [NameID]", symbolId);
				foreach (object[] n in names)
				{
					fields[Db.L(n[0])] = db.Query("SELECT [AttName],[AttValue],[ShowAtt] FROM [Attribute] WHERE [NameID]=? ORDER BY [AttributeID]", n[0]);
				}
			}

			List<long> ids = new List<long>();
			Backup();
			using (Db db = new Db(Path))
			{
				db.Transaction(delegate
				{
					EnsureConnectorColumn(db);
					db.Exec("INSERT INTO [Symbol] ([Data]) VALUES (?)", data);
					long newSymbol = db.LastId;
					foreach (object[] n in names)
					{
						db.Exec("INSERT INTO [Name] ([Name],[SymbolID],[Type],[Reference],[ppp],[Description],[ShowName],[ShowRef],[DefRotate],[IsConnector])"
							+ " VALUES (?,?,?,?,?,?,?,?,?,?)",
							n[1], newSymbol, n[2], n[3], n[4], n[5], n[6], n[7], n[8], n[9]);
						long newName = db.LastId;
						ids.Add(newName);
						foreach (object[] a in fields[Db.L(n[0])])
						{
							db.Exec("INSERT INTO [Attribute] ([NameID],[AttName],[AttValue],[ShowAtt]) VALUES (?,?,?,?)",
								newName, a[0], a[1], a[2]);
						}
					}
				});
			}
			return ids;
		}

		public void Delete(long symbolId)
		{
			Backup();
			using (Db db = new Db(Path))
			{
				db.Transaction(delegate
				{
					db.Exec("DELETE FROM [Attribute] WHERE [NameID] IN (SELECT [NameID] FROM [Name] WHERE [SymbolID]=?)", symbolId);
					db.Exec("DELETE FROM [Name] WHERE [SymbolID]=?", symbolId);
					db.Exec("DELETE FROM [Symbol] WHERE [SymbolID]=?", symbolId);
				});
			}
		}

		// An empty library, the same tables as CLibrarySQLite::Create
		public static void Create(string path)
		{
			if (File.Exists(path))
			{
				throw new Exception("The file already exists:\n" + path);
			}
			using (Db db = new Db(path))
			{
				db.Transaction(delegate
				{
					db.Exec("CREATE TABLE [Name] ([NameID] INTEGER PRIMARY KEY,[Name] TEXT,[SymbolID] INTEGER,[Type] INTEGER,"
						+ "[Reference] TEXT,[ppp] INTEGER,[Description] TEXT,[ShowName] INTEGER,[ShowRef] INTEGER,"
						+ "[DefRotate] INTEGER,[IsConnector] INTEGER DEFAULT 0)");
					db.Exec("CREATE TABLE [Symbol] ([SymbolID] INTEGER PRIMARY KEY,[Data] TEXT)");
					db.Exec("CREATE TABLE [Attribute] ([AttributeID] INTEGER PRIMARY KEY,[NameID] INTEGER,[AttName] TEXT,[AttValue] TEXT,[ShowAtt] INTEGER)");
					db.Exec("CREATE INDEX [idx_NameID] ON [Attribute] ([NameID])");
				});
			}
		}
	}

	// ------------------------------------------------------------------
	// Where ConCAD keeps its libraries

	static class Places
	{
		const string ConCadKey = @"Software\ConCAD\ConCAD\1x20";
		const string MyKey = @"Software\ConCAD\LibraryOrganizer";

		public static string LibraryFolder
		{
			get { return Path.Combine(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "ConCAD"), "library"); }
		}

		// ConCAD's library lists, then anything else in the library folder
		public static List<string> FindLibraries()
		{
			List<string> found = new List<string>();
			using (RegistryKey k = Registry.CurrentUser.OpenSubKey(ConCadKey))
			{
				if (k != null)
				{
					AddList(found, k.GetValue("Libraries") as string, ".TCLib", ".CCMod");
					AddList(found, k.GetValue("ModuleLibraries") as string, ".CCMod", ".TCLib");
				}
			}
			if (Directory.Exists(LibraryFolder))
			{
				foreach (string pattern in new string[] { "*.TCLib", "*.CCMod" })
				{
					foreach (string f in Directory.GetFiles(LibraryFolder, pattern))
					{
						Add(found, f);
					}
				}
			}
			return found;
		}

		// ConCAD stores the paths without extension
		static void AddList(List<string> found, string list, string ext, string otherExt)
		{
			if (string.IsNullOrEmpty(list))
			{
				return;
			}
			foreach (string p in list.Split(','))
			{
				string name = p.Trim();
				if (name.Length == 0)
				{
					continue;
				}
				if (File.Exists(name + ext))
				{
					Add(found, name + ext);
				}
				else if (File.Exists(name + otherExt))
				{
					Add(found, name + otherExt);
				}
			}
		}

		public static void Add(List<string> list, string path)
		{
			string full = Path.GetFullPath(path);
			if (!list.Any(p => string.Equals(p, full, StringComparison.OrdinalIgnoreCase)))
			{
				list.Add(full);
			}
		}

		// Adds a new library to ConCAD's list (only while ConCAD is closed:
		// ConCAD writes its own list back when it exits)
		public static void AddToConCad(string path, bool modules)
		{
			using (RegistryKey k = Registry.CurrentUser.CreateSubKey(ConCadKey))
			{
				string value = modules ? "ModuleLibraries" : "Libraries";
				string list = k.GetValue(value) as string ?? "";
				string name = Path.Combine(Path.GetDirectoryName(path), Path.GetFileNameWithoutExtension(path));
				k.SetValue(value, list.Length == 0 ? name : list + "," + name);
				if (modules && string.IsNullOrEmpty(k.GetValue("ModuleLibrary") as string))
				{
					k.SetValue("ModuleLibrary", name);
				}
			}
		}

		public static string GetSetting(string name)
		{
			using (RegistryKey k = Registry.CurrentUser.OpenSubKey(MyKey))
			{
				return k == null ? "" : (k.GetValue(name) as string ?? "");
			}
		}

		public static void SetSetting(string name, string value)
		{
			using (RegistryKey k = Registry.CurrentUser.CreateSubKey(MyKey))
			{
				k.SetValue(name, value ?? "");
			}
		}

		public static bool ConCadRunning()
		{
			Process[] p = Process.GetProcessesByName("ConCAD");
			foreach (Process x in p)
			{
				x.Dispose();
			}
			return p.Length > 0;
		}
	}

	// ------------------------------------------------------------------
	// One side of the window: a library and its contents

	sealed class DragItems
	{
		public LibraryPane From;
	}

	sealed class LibraryPane : TableLayoutPanel
	{
		readonly MainForm owner;
		public readonly ComboBox Picker = new ComboBox();
		readonly Label pathLabel = new Label();
		readonly TextBox search = new TextBox();
		public readonly ListView ItemList = new ListView();
		readonly Label count = new Label();
		List<Entry> entries = new List<Entry>();
		int sortColumn;
		bool ignorePick;

		public Library Library;

		public LibraryPane(MainForm owner)
		{
			this.owner = owner;
			Dock = DockStyle.Fill;
			ColumnCount = 1;
			RowCount = 5;
			RowStyles.Add(new RowStyle(SizeType.AutoSize));
			RowStyles.Add(new RowStyle(SizeType.AutoSize));
			RowStyles.Add(new RowStyle(SizeType.AutoSize));
			RowStyles.Add(new RowStyle(SizeType.Percent, 100));
			RowStyles.Add(new RowStyle(SizeType.AutoSize));

			TableLayoutPanel top = new TableLayoutPanel();
			top.Dock = DockStyle.Fill;
			top.AutoSize = true;
			top.ColumnCount = 3;
			top.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
			top.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
			top.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
			top.Margin = new Padding(0);
			Picker.DropDownStyle = ComboBoxStyle.DropDownList;
			Picker.Dock = DockStyle.Fill;
			Picker.SelectedIndexChanged += delegate { if (!ignorePick) owner.PickLibrary(this, Picker.SelectedItem as Library); };
			Button open = new Button();
			open.Text = "Open...";
			open.AutoSize = true;
			open.Click += delegate { owner.OpenLibrary(this); };
			Button create = new Button();
			create.Text = "New...";
			create.AutoSize = true;
			create.Click += delegate { owner.NewLibrary(this); };
			owner.Tips.SetToolTip(open, "Open another library file (.TCLib or .CCMod)");
			owner.Tips.SetToolTip(create, "Create a new, empty library");
			top.Controls.Add(Picker, 0, 0);
			top.Controls.Add(open, 1, 0);
			top.Controls.Add(create, 2, 0);
			Controls.Add(top, 0, 0);

			pathLabel.AutoSize = true;
			pathLabel.ForeColor = SystemColors.GrayText;
			pathLabel.Margin = new Padding(3, 0, 3, 4);
			Controls.Add(pathLabel, 0, 1);

			search.Dock = DockStyle.Fill;
			search.TextChanged += delegate { Fill(null); };
			SetCue(search, "Search...");
			Controls.Add(search, 0, 2);

			ItemList.Dock = DockStyle.Fill;
			ItemList.View = View.Details;
			ItemList.FullRowSelect = true;
			ItemList.MultiSelect = true;
			ItemList.HideSelection = false;
			ItemList.AllowDrop = true;
			ItemList.SelectedIndexChanged += delegate { owner.SelectionChanged(this); };
			ItemList.ColumnClick += delegate(object s, ColumnClickEventArgs e) { sortColumn = e.Column; Fill(SelectedNameIds()); };
			ItemList.ItemDrag += delegate
			{
				DragItems d = new DragItems();
				d.From = this;
				ItemList.DoDragDrop(d, DragDropEffects.Move | DragDropEffects.Copy);
			};
			ItemList.DragOver += OnDragOver;
			ItemList.DragEnter += OnDragOver;
			ItemList.DragDrop += delegate(object s, DragEventArgs e)
			{
				DragItems d = e.Data.GetData(typeof(DragItems)) as DragItems;
				if (d != null && d.From != this)
				{
					owner.Transfer(d.From, this, e.Effect == DragDropEffects.Move);
				}
			};
			ItemList.KeyDown += delegate(object s, KeyEventArgs e)
			{
				if (e.KeyCode == Keys.Delete)
				{
					owner.DeleteSelected(this);
				}
				else if (e.KeyCode == Keys.A && e.Control)
				{
					foreach (ListViewItem i in ItemList.Items)
					{
						i.Selected = true;
					}
				}
			};
			Controls.Add(ItemList, 0, 3);

			count.AutoSize = true;
			count.ForeColor = SystemColors.GrayText;
			Controls.Add(count, 0, 4);
		}

		void OnDragOver(object sender, DragEventArgs e)
		{
			DragItems d = e.Data.GetData(typeof(DragItems)) as DragItems;
			if (d == null || d.From == this || Library == null)
			{
				e.Effect = DragDropEffects.None;
			}
			else
			{
				e.Effect = (e.KeyState & 8) != 0 ? DragDropEffects.Copy : DragDropEffects.Move;  // Ctrl = copy
			}
		}

		[DllImport("user32.dll", CharSet = CharSet.Unicode)]
		static extern IntPtr SendMessage(IntPtr hWnd, int msg, IntPtr wParam, string lParam);

		static void SetCue(TextBox box, string text)
		{
			box.HandleCreated += delegate { SendMessage(box.Handle, 0x1501, (IntPtr) 1, text); }; // EM_SETCUEBANNER
		}

		public void SetLibraries(List<Library> all, Library select)
		{
			ignorePick = true;
			Picker.Items.Clear();
			foreach (Library l in all)
			{
				Picker.Items.Add(l);
			}
			Picker.SelectedItem = select;
			ignorePick = false;
		}

		public List<Entry> Entries
		{
			get { return entries; }
		}

		public List<Entry> SelectedEntries
		{
			get { return ItemList.SelectedItems.Cast<ListViewItem>().Select(i => (Entry) i.Tag).ToList(); }
		}

		public List<long> SelectedNameIds()
		{
			return SelectedEntries.Select(e => e.NameId).ToList();
		}

		// Shows library (or nothing); returns false if it cannot be read
		public bool ShowLibrary(Library library)
		{
			Library = library;
			entries = new List<Entry>();
			bool ok = true;
			if (library != null)
			{
				try
				{
					entries = library.Load();
				}
				catch (Exception ex)
				{
					ok = false;
					MessageBox.Show(owner, "Cannot read " + library.Path + "\n\n" + ex.Message + "\n\nIs it a ConCAD library?",
						MainForm.AppName, MessageBoxButtons.OK, MessageBoxIcon.Warning);
				}
			}
			pathLabel.Text = library == null ? "" : library.Path;
			owner.Tips.SetToolTip(pathLabel, pathLabel.Text);
			ItemList.Columns.Clear();
			if (library != null && library.ForModules)
			{
				ItemList.Columns.Add("Type", 150);
				ItemList.Columns.Add("Name", 220);
				ItemList.Columns.Add("Partnumber", 130);
			}
			else
			{
				ItemList.Columns.Add("Name", 200);
				ItemList.Columns.Add("Reference", 80);
				ItemList.Columns.Add("Description", 260);
				ItemList.Columns.Add("Connector", 70);
			}
			sortColumn = 0;
			Fill(null);
			return ok;
		}

		public void Reload(List<long> select)
		{
			ShowLibrary(Library);
			if (select != null)
			{
				Fill(select);
			}
		}

		string[] Columns(Entry e)
		{
			if (Library != null && Library.ForModules)
			{
				return new string[] { e.IsModule ? e.ModuleType : "(symbol)", e.IsModule ? e.ModuleName : e.Name, e.FieldValue("Partnumber") };
			}
			return new string[] { e.IsModule ? e.DisplayName : e.Name, e.Reference, e.IsModule ? "(module)" : e.Description, e.Connector ? "Yes" : "" };
		}

		// Fills the list (filtered and sorted), selecting the given names
		// (or keeping the current selection)
		void Fill(List<long> select)
		{
			if (select == null)
			{
				select = SelectedNameIds();
			}
			string filter = search.Text.Trim();
			List<KeyValuePair<Entry, string[]>> rows = new List<KeyValuePair<Entry, string[]>>();
			foreach (Entry e in entries)
			{
				string[] cols = Columns(e);
				if (filter.Length > 0 && !Matches(e, cols, filter))
				{
					continue;
				}
				rows.Add(new KeyValuePair<Entry, string[]>(e, cols));
			}
			int c = sortColumn;
			rows.Sort((a, b) =>
			{
				int r = string.Compare(a.Value[c], b.Value[c], StringComparison.CurrentCultureIgnoreCase);
				return r != 0 ? r : string.Compare(a.Key.DisplayName, b.Key.DisplayName, StringComparison.CurrentCultureIgnoreCase);
			});

			owner.Filling = true;
			ItemList.BeginUpdate();
			ItemList.Items.Clear();
			ListViewItem first = null;
			foreach (KeyValuePair<Entry, string[]> row in rows)
			{
				ListViewItem item = new ListViewItem(row.Value);
				item.Tag = row.Key;
				ItemList.Items.Add(item);
				if (select.Contains(row.Key.NameId))
				{
					item.Selected = true;
					if (first == null)
					{
						first = item;
					}
				}
			}
			ItemList.EndUpdate();
			owner.Filling = false;
			if (first != null)
			{
				first.EnsureVisible();
				first.Focused = true;
			}

			string what = Library == null ? "" : Library.Kind;
			count.Text = Library == null ? "No library chosen"
				: rows.Count == entries.Count ? entries.Count + " " + what
				: rows.Count + " of " + entries.Count + " " + what + " shown";
			owner.UpdateButtons();
		}

		static bool Matches(Entry e, string[] cols, string filter)
		{
			foreach (string s in cols)
			{
				if (s.IndexOf(filter, StringComparison.CurrentCultureIgnoreCase) >= 0)
				{
					return true;
				}
			}
			foreach (Field f in e.Fields)
			{
				if (f.Value.IndexOf(filter, StringComparison.CurrentCultureIgnoreCase) >= 0)
				{
					return true;
				}
			}
			return false;
		}
	}

	// ------------------------------------------------------------------
	// The window

	sealed class MainForm : Form
	{
		public const string AppName = "ConCAD Library Organizer";
		public readonly ToolTip Tips = new ToolTip();
		public bool Filling;

		readonly List<Library> libraries = new List<Library>();
		readonly LibraryPane left;
		readonly LibraryPane right;
		readonly Label running = new Label();
		readonly ToolStripStatusLabel status = new ToolStripStatusLabel();
		readonly Button moveRight = new Button();
		readonly Button copyRight = new Button();
		readonly Button moveLeft = new Button();
		readonly Button copyLeft = new Button();
		readonly Button delete = new Button();

		// Properties
		readonly GroupBox propsBox = new GroupBox();
		readonly Label hint = new Label();
		readonly TableLayoutPanel props = new TableLayoutPanel();
		readonly Label nameLabel = new Label();
		readonly TextBox nameBox = new TextBox();
		readonly Label typeLabel = new Label();
		readonly TextBox typeBox = new TextBox();
		readonly Label refLabel = new Label();
		readonly TextBox refBox = new TextBox();
		readonly Label descLabel = new Label();
		readonly TextBox descBox = new TextBox();
		readonly CheckBox connectorBox = new CheckBox();
		readonly DataGridView grid = new DataGridView();
		readonly DataGridViewComboBoxColumn showColumn = new DataGridViewComboBoxColumn();
		readonly Button save = new Button();
		readonly Button undo = new Button();
		static readonly string[] ShowNames = { "Shown", "Hidden", "Never shown" };

		Entry editing;
		LibraryPane editingPane;
		List<Field> hiddenFields = new List<Field>();
		bool loading;
		bool dirty;
		LibraryPane lastPane;

		public MainForm()
		{
			Text = AppName;
			Font = new Font("Segoe UI", 9F);
			Size = new Size(1250, 850);
			MinimumSize = new Size(800, 550);
			StartPosition = FormStartPosition.CenterScreen;
			try { Icon = Icon.ExtractAssociatedIcon(Application.ExecutablePath); } catch { }

			left = new LibraryPane(this);
			right = new LibraryPane(this);

			// Warning while ConCAD is running
			running.Dock = DockStyle.Top;
			running.AutoSize = false;
			running.Height = 30;
			running.TextAlign = ContentAlignment.MiddleLeft;
			running.BackColor = Color.FromArgb(255, 236, 160);
			running.Padding = new Padding(8, 0, 8, 0);
			running.Text = "ConCAD is running. Close it before you change libraries: ConCAD only sees the changes after a restart, and it could overwrite them.";
			running.Visible = false;

			// The two libraries with the buttons between them
			FlowLayoutPanel middle = new FlowLayoutPanel();
			middle.FlowDirection = FlowDirection.TopDown;
			middle.Dock = DockStyle.Fill;
			middle.Padding = new Padding(0, 70, 0, 0);
			middle.WrapContents = false;
			SetupButton(moveRight, "Move  →", "Move the selected items to the library on the right (or drag them)");
			SetupButton(copyRight, "Copy  →", "Copy the selected items to the library on the right (or Ctrl+drag them)");
			SetupButton(moveLeft, "←  Move", "Move the selected items to the library on the left (or drag them)");
			SetupButton(copyLeft, "←  Copy", "Copy the selected items to the library on the left (or Ctrl+drag them)");
			SetupButton(delete, "Delete", "Delete the selected items (Delete key)");
			moveLeft.Margin = new Padding(3, 30, 3, 3);
			delete.Margin = new Padding(3, 30, 3, 3);
			moveRight.Click += delegate { Transfer(left, right, true); };
			copyRight.Click += delegate { Transfer(left, right, false); };
			moveLeft.Click += delegate { Transfer(right, left, true); };
			copyLeft.Click += delegate { Transfer(right, left, false); };
			delete.Click += delegate { DeleteSelected(lastPane); };
			middle.Controls.AddRange(new Control[] { moveRight, copyRight, moveLeft, copyLeft, delete });

			TableLayoutPanel lists = new TableLayoutPanel();
			lists.Dock = DockStyle.Fill;
			lists.ColumnCount = 3;
			lists.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
			lists.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 104));
			lists.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 50));
			lists.Controls.Add(left, 0, 0);
			lists.Controls.Add(middle, 1, 0);
			lists.Controls.Add(right, 2, 0);

			BuildProperties();

			SplitContainer split = new SplitContainer();
			split.Dock = DockStyle.Fill;
			split.Orientation = Orientation.Horizontal;
			split.Panel1.Controls.Add(lists);
			split.Panel2.Controls.Add(propsBox);
			split.Panel2.Padding = new Padding(6, 0, 6, 6);

			StatusStrip strip = new StatusStrip();
			status.Spring = true;
			status.TextAlign = ContentAlignment.MiddleLeft;
			strip.Items.Add(status);

			Controls.Add(split);
			Controls.Add(running);
			Controls.Add(strip);

			Load += delegate
			{
				split.SplitterDistance = (int) (split.Height * 0.55);
				LoadLibraries();
				CheckConCad();
			};
			Timer timer = new Timer();
			timer.Interval = 2000;
			timer.Tick += delegate { CheckConCad(); };
			timer.Start();

			FormClosing += delegate(object s, FormClosingEventArgs e)
			{
				if (!AskSave(true))
				{
					e.Cancel = true;
					return;
				}
				Places.SetSetting("Left", left.Library == null ? "" : left.Library.Path);
				Places.SetSetting("Right", right.Library == null ? "" : right.Library.Path);
				Places.SetSetting("Opened", string.Join("|", libraries.Select(l => l.Path).ToArray()));
			};

			ShowEntry(null, null);
			UpdateButtons();
		}

		void SetupButton(Button b, string text, string tip)
		{
			b.Text = text;
			b.Width = 92;
			b.Height = 30;
			Tips.SetToolTip(b, tip);
		}

		void BuildProperties()
		{
			propsBox.Dock = DockStyle.Fill;
			propsBox.Text = "Properties";
			propsBox.Padding = new Padding(8);

			props.Dock = DockStyle.Fill;
			props.ColumnCount = 2;
			props.ColumnStyles.Add(new ColumnStyle(SizeType.AutoSize));
			props.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));

			int row = 0;
			AddRow(nameLabel, "Name", nameBox, ref row);
			AddRow(typeLabel, "Type", typeBox, ref row);
			AddRow(refLabel, "Reference", refBox, ref row);
			AddRow(descLabel, "Description", descBox, ref row);
			connectorBox.Text = "Connector (each placed copy can get its own colour)";
			connectorBox.AutoSize = true;
			props.RowStyles.Add(new RowStyle(SizeType.AutoSize));
			props.Controls.Add(connectorBox, 1, row++);

			Label fieldsLabel = new Label();
			fieldsLabel.Text = "Fields";
			fieldsLabel.AutoSize = true;
			fieldsLabel.Margin = new Padding(3, 8, 3, 0);
			props.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
			props.Controls.Add(fieldsLabel, 0, row);

			grid.Dock = DockStyle.Fill;
			grid.AllowUserToAddRows = true;
			grid.AllowUserToDeleteRows = true;
			grid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
			grid.RowHeadersWidth = 28;
			grid.BackgroundColor = SystemColors.Window;
			grid.EditMode = DataGridViewEditMode.EditOnKeystrokeOrF2;
			grid.Columns.Add("field", "Field");
			grid.Columns.Add("value", "Value");
			grid.Columns[0].FillWeight = 30;
			grid.Columns[1].FillWeight = 55;
			showColumn.HeaderText = "Visibility";
			showColumn.Items.AddRange(ShowNames);
			showColumn.FillWeight = 15;
			showColumn.DisplayStyle = DataGridViewComboBoxDisplayStyle.DropDownButton;
			showColumn.FlatStyle = FlatStyle.Flat;
			grid.Columns.Add(showColumn);
			grid.CellValueChanged += delegate { Changed(); };
			grid.UserDeletedRow += delegate { Changed(); };
			grid.CurrentCellDirtyStateChanged += delegate
			{
				if (grid.IsCurrentCellDirty && grid.CurrentCell is DataGridViewComboBoxCell)
				{
					grid.CommitEdit(DataGridViewDataErrorContexts.Commit);
				}
			};
			grid.DefaultValuesNeeded += delegate(object s, DataGridViewRowEventArgs e) { e.Row.Cells[2].Value = ShowNames[2]; };   // new fields: Never shown
			grid.DataError += delegate(object s, DataGridViewDataErrorEventArgs e) { e.ThrowException = false; };
			props.Controls.Add(grid, 1, row++);

			hint.Text = "Type in the empty last row to add a field. Click the grey box at the left of a row and press Delete to remove a field.";
			hint.AutoSize = true;
			hint.ForeColor = SystemColors.GrayText;
			props.RowStyles.Add(new RowStyle(SizeType.AutoSize));
			props.Controls.Add(hint, 1, row++);

			FlowLayoutPanel buttons = new FlowLayoutPanel();
			buttons.AutoSize = true;
			buttons.Dock = DockStyle.Fill;
			save.Text = "Save changes";
			save.AutoSize = true;
			save.Click += delegate { SaveEditing(); };
			undo.Text = "Undo changes";
			undo.AutoSize = true;
			undo.Click += delegate { ShowEntry(editingPane, editing); };
			buttons.Controls.Add(save);
			buttons.Controls.Add(undo);
			props.RowStyles.Add(new RowStyle(SizeType.AutoSize));
			props.Controls.Add(buttons, 1, row++);

			foreach (TextBox t in new TextBox[] { nameBox, typeBox, refBox, descBox })
			{
				t.TextChanged += delegate { Changed(); };
			}
			connectorBox.CheckedChanged += delegate { Changed(); };

			propsBox.Controls.Add(props);
		}

		void AddRow(Label label, string text, TextBox box, ref int row)
		{
			label.Text = text;
			label.AutoSize = true;
			label.Anchor = AnchorStyles.Left;
			box.Dock = DockStyle.Fill;
			props.RowStyles.Add(new RowStyle(SizeType.AutoSize));
			props.Controls.Add(label, 0, row);
			props.Controls.Add(box, 1, row);
			row++;
		}

		void CheckConCad()
		{
			bool r = Places.ConCadRunning();
			if (running.Visible != r)
			{
				running.Visible = r;
			}
		}

		// ---- Libraries

		void LoadLibraries()
		{
			foreach (string p in Places.FindLibraries())
			{
				AddLibrary(p);
			}
			foreach (string p in Places.GetSetting("Opened").Split('|'))
			{
				if (p.Length > 0 && File.Exists(p))
				{
					AddLibrary(p);
				}
			}
			Library l = FindLibrary(Places.GetSetting("Left"));
			Library r = FindLibrary(Places.GetSetting("Right"));
			if (l == null && libraries.Count > 0)
			{
				l = libraries[0];
			}
			if (r == null)
			{
				r = libraries.FirstOrDefault(x => x != l && x.ForModules == (l != null && l.ForModules)) ?? libraries.FirstOrDefault(x => x != l);
			}
			left.SetLibraries(libraries, l);
			right.SetLibraries(libraries, r);
			left.ShowLibrary(l);
			right.ShowLibrary(r);
			status.Text = libraries.Count == 0
				? "No libraries found in " + Places.LibraryFolder + ". Use Open... to choose one."
				: "Pick a library on each side. Select an item to edit its properties; drag items to move them (Ctrl+drag copies).";
		}

		Library FindLibrary(string path)
		{
			return libraries.FirstOrDefault(x => string.Equals(x.Path, path, StringComparison.OrdinalIgnoreCase));
		}

		Library AddLibrary(string path)
		{
			string full = Path.GetFullPath(path);
			Library l = FindLibrary(full);
			if (l == null)
			{
				l = new Library(full);
				libraries.Add(l);
			}
			return l;
		}

		void RefreshPickers(LibraryPane pane, Library select)
		{
			left.SetLibraries(libraries, pane == left ? select : left.Library);
			right.SetLibraries(libraries, pane == right ? select : right.Library);
		}

		public void PickLibrary(LibraryPane pane, Library library)
		{
			if (library == pane.Library)
			{
				return;
			}
			if (editingPane == pane && !AskSave(false))
			{
				RefreshPickers(pane, pane.Library);
				return;
			}
			pane.ShowLibrary(library);
		}

		public void OpenLibrary(LibraryPane pane)
		{
			using (OpenFileDialog d = new OpenFileDialog())
			{
				d.Title = "Open a ConCAD library";
				d.Filter = "ConCAD libraries (*.TCLib;*.CCMod)|*.TCLib;*.CCMod|All files (*.*)|*.*";
				d.InitialDirectory = Directory.Exists(Places.LibraryFolder) ? Places.LibraryFolder : "";
				if (d.ShowDialog(this) != DialogResult.OK)
				{
					return;
				}
				if (editingPane == pane && !AskSave(false))
				{
					return;
				}
				Library l = AddLibrary(d.FileName);
				RefreshPickers(pane, l);
				pane.ShowLibrary(l);
			}
		}

		public void NewLibrary(LibraryPane pane)
		{
			using (SaveFileDialog d = new SaveFileDialog())
			{
				d.Title = "New library";
				d.Filter = "Symbol library (*.TCLib)|*.TCLib|Module library (*.CCMod)|*.CCMod";
				d.FilterIndex = pane.Library != null && pane.Library.ForModules ? 2 : 1;
				d.InitialDirectory = Directory.Exists(Places.LibraryFolder) ? Places.LibraryFolder : "";
				d.OverwritePrompt = false;
				if (d.ShowDialog(this) != DialogResult.OK)
				{
					return;
				}
				if (editingPane == pane && !AskSave(false))
				{
					return;
				}
				try
				{
					Library.Create(d.FileName);
				}
				catch (Exception ex)
				{
					Error("The library could not be created.", ex);
					return;
				}
				Library l = AddLibrary(d.FileName);
				RefreshPickers(pane, l);
				pane.ShowLibrary(l);
				status.Text = "Created " + l.Path;

				if (Places.ConCadRunning())
				{
					MessageBox.Show(this, "The library was created. To use it in ConCAD, add it there with Library → Libraries... (Add).",
						AppName, MessageBoxButtons.OK, MessageBoxIcon.Information);
				}
				else if (MessageBox.Show(this, "Add \"" + l.Title + "\" to ConCAD's list of libraries, so ConCAD uses it?",
					AppName, MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
				{
					try
					{
						Places.AddToConCad(l.Path, l.ForModules);
					}
					catch (Exception ex)
					{
						Error("The library could not be added to ConCAD's list.", ex);
					}
				}
			}
		}

		// Reloads every side that shows library (both may show the same one)
		void Reload(Library library, LibraryPane pane, List<long> select)
		{
			foreach (LibraryPane p in new LibraryPane[] { left, right })
			{
				if (p.Library == library)
				{
					p.Reload(p == pane ? select : p.SelectedNameIds());
				}
			}
		}

		// ---- Move, copy and delete

		static List<Entry> OnePerSymbol(List<Entry> list)
		{
			// Names of one symbol share its drawing and always travel together
			return list.GroupBy(e => e.SymbolId).Select(g => g.First()).ToList();
		}

		public void Transfer(LibraryPane from, LibraryPane to, bool move)
		{
			if (from.Library == null || to.Library == null)
			{
				Info("Choose a library on both sides first.");
				return;
			}
			if (from.Library == to.Library)
			{
				Info("Both sides show the same library. Choose another library on one side.");
				return;
			}
			List<Entry> items = OnePerSymbol(from.SelectedEntries);
			if (items.Count == 0)
			{
				Info("Select the symbols or modules to " + (move ? "move" : "copy") + " in the " + (from == left ? "left" : "right") + " list first.");
				return;
			}
			if (items.Any(e => e.IsModule != to.Library.ForModules))
			{
				Info("Modules can only go into module libraries (.CCMod), and symbols only into symbol libraries (.TCLib).\n\n"
					+ "\"" + to.Library.Title + "\" is a " + (to.Library.ForModules ? "module" : "symbol") + " library.");
				return;
			}
			if (!AskSave(false))
			{
				return;
			}

			HashSet<string> existing = new HashSet<string>(to.Entries.Select(e => e.DisplayName), StringComparer.CurrentCultureIgnoreCase);
			List<string> dups = items.Where(e => existing.Contains(e.DisplayName)).Select(e => e.DisplayName).ToList();
			if (dups.Count > 0)
			{
				string names = string.Join("\n", dups.Take(15).ToArray()) + (dups.Count > 15 ? "\n..." : "");
				if (MessageBox.Show(this, "These are already in \"" + to.Library.Title + "\":\n\n" + names + "\n\n"
					+ (move ? "Move" : "Copy") + " anyway? The library will then have both.",
					AppName, MessageBoxButtons.YesNo, MessageBoxIcon.Question) != DialogResult.Yes)
				{
					return;
				}
			}

			List<long> added = new List<long>();
			int done = 0;
			Cursor = Cursors.WaitCursor;
			try
			{
				foreach (Entry e in items)
				{
					added.AddRange(to.Library.CopyFrom(from.Library, e.SymbolId));
					if (move)
					{
						from.Library.Delete(e.SymbolId);
					}
					done++;
				}
			}
			catch (Exception ex)
			{
				Error("Stopped after " + done + " of " + items.Count + ".", ex);
			}
			finally
			{
				Cursor = Cursors.Default;
				ShowEntry(null, null);
				Reload(from.Library, from, new List<long>());
				Reload(to.Library, to, added);
			}
			status.Text = (move ? "Moved " : "Copied ") + Count(done, items) + " from " + from.Library.Title + " to " + to.Library.Title + "."
				+ BackupNote(from.Library, to.Library, move);
		}

		static string Count(int n, List<Entry> items)
		{
			bool modules = items.Count > 0 && items[0].IsModule;
			return n + " " + (modules ? (n == 1 ? "module" : "modules") : (n == 1 ? "symbol" : "symbols"));
		}

		static string BackupNote(Library a, Library b, bool both)
		{
			return "  The libraries as they were when you started are kept as " + (both ? Path.GetFileName(a.BackupPath) + " and " : "")
				+ Path.GetFileName(b.BackupPath) + ".";
		}

		public void DeleteSelected(LibraryPane pane)
		{
			if (pane == null || pane.Library == null)
			{
				Info("Select the items to delete first.");
				return;
			}
			List<Entry> items = OnePerSymbol(pane.SelectedEntries);
			if (items.Count == 0)
			{
				Info("Select the items to delete first.");
				return;
			}
			string names = string.Join("\n", items.Take(15).Select(e => e.DisplayName).ToArray()) + (items.Count > 15 ? "\n..." : "");
			if (MessageBox.Show(this, "Delete these from \"" + pane.Library.Title + "\"?\n\n" + names,
				AppName, MessageBoxButtons.YesNo, MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2) != DialogResult.Yes)
			{
				return;
			}
			if (!AskSave(false))
			{
				return;
			}
			ShowEntry(null, null);
			int done = 0;
			try
			{
				foreach (Entry e in items)
				{
					pane.Library.Delete(e.SymbolId);
					done++;
				}
			}
			catch (Exception ex)
			{
				Error("Stopped after " + done + " of " + items.Count + ".", ex);
			}
			Reload(pane.Library, pane, new List<long>());
			status.Text = "Deleted " + Count(done, items) + " from " + pane.Library.Title + "."
				+ "  The library as it was when you started is kept as " + Path.GetFileName(pane.Library.BackupPath) + ".";
		}

		// ---- Selection and the properties editor

		public void SelectionChanged(LibraryPane pane)
		{
			if (Filling)
			{
				return;
			}
			lastPane = pane;
			UpdateButtons();
			// The list sends one event per item; act once it has settled
			BeginInvoke(new Action(delegate { AfterSelection(pane); }));
		}

		void AfterSelection(LibraryPane pane)
		{
			List<Entry> sel = pane.SelectedEntries;
			if (sel.Count == 1 && editing != null && pane == editingPane && sel[0].NameId == editing.NameId)
			{
				return;
			}
			if (sel.Count == 0 && pane != editingPane)
			{
				return;   // the other side was cleared: keep editing
			}
			if (!AskSave(false))
			{
				return;
			}
			ShowEntry(pane, sel.Count == 1 ? sel[0] : null);
			if (sel.Count > 1)
			{
				hint.Text = sel.Count + " items selected. Select one item to edit its properties.";
			}
		}

		public void UpdateButtons()
		{
			bool l = left.ItemList.SelectedItems.Count > 0;
			bool r = right.ItemList.SelectedItems.Count > 0;
			moveRight.Enabled = copyRight.Enabled = l && right.Library != null;
			moveLeft.Enabled = copyLeft.Enabled = r && left.Library != null;
			delete.Enabled = lastPane != null && lastPane.ItemList.SelectedItems.Count > 0;
		}

		void ShowEntry(LibraryPane pane, Entry e)
		{
			loading = true;
			editing = e;
			editingPane = e == null ? null : pane;
			dirty = false;
			hiddenFields = new List<Field>();
			grid.Rows.Clear();

			bool module = e != null && e.IsModule;
			typeLabel.Visible = typeBox.Visible = module;
			refLabel.Visible = refBox.Visible = !module;   // modules have no Reference
			descLabel.Visible = descBox.Visible = !module;
			connectorBox.Visible = !module;
			showColumn.Visible = !module;

			if (e == null)
			{
				propsBox.Text = "Properties";
				nameBox.Text = typeBox.Text = refBox.Text = descBox.Text = "";
				connectorBox.Checked = false;
				hint.Text = "Select a symbol or module to edit its properties.";
				props.Enabled = true;
				foreach (Control c in props.Controls)
				{
					if (c != hint)
					{
						c.Enabled = false;
					}
				}
			}
			else
			{
				foreach (Control c in props.Controls)
				{
					c.Enabled = true;
				}
				propsBox.Text = (module ? "Module" : "Symbol") + " \"" + e.DisplayName + "\" in " + pane.Library.Title;
				hint.Text = "Type in the empty last row to add a field. Click the grey box at the left of a row and press Delete to remove a field.";
				refBox.Text = e.Reference;
				if (module)
				{
					nameBox.Text = e.ModuleName;
					typeBox.Text = e.ModuleType;
					if (e.Find("Partnumber") == null)
					{
						AddGridRow("Partnumber", "", 0);
					}
				}
				else
				{
					nameBox.Text = e.Name;
					descBox.Text = e.Description;
					connectorBox.Checked = e.Connector;
				}
				foreach (Field f in e.Fields)
				{
					bool shownInBox = module && (string.Equals(f.Name, "Name", StringComparison.OrdinalIgnoreCase)
						|| string.Equals(f.Name, "Type", StringComparison.OrdinalIgnoreCase));
					if (shownInBox || Entry.IsObsolete(f))
					{
						if (!shownInBox)
						{
							hiddenFields.Add(f);
						}
						continue;
					}
					AddGridRow(f.Name, f.Value, f.Show);
				}
			}
			loading = false;
			UpdateSaveButtons();
		}

		void AddGridRow(string name, string value, int show)
		{
			grid.Rows.Add(name, value, ShowNames[Math.Max(0, Math.Min(2, show))]);
		}

		void Changed()
		{
			if (!loading && editing != null)
			{
				dirty = true;
				UpdateSaveButtons();
			}
		}

		void UpdateSaveButtons()
		{
			save.Enabled = undo.Enabled = editing != null && dirty;
		}

		// Asks whether to save unsaved edits; false = the user cancelled
		bool AskSave(bool canCancel)
		{
			grid.EndEdit();
			if (editing == null || !dirty)
			{
				return true;
			}
			DialogResult r = MessageBox.Show(this, "Save the changes to \"" + editing.DisplayName + "\"?", AppName,
				canCancel ? MessageBoxButtons.YesNoCancel : MessageBoxButtons.YesNo, MessageBoxIcon.Question);
			if (r == DialogResult.Cancel)
			{
				return false;
			}
			if (r == DialogResult.Yes)
			{
				return SaveEditing();
			}
			dirty = false;
			return true;
		}

		bool SaveEditing()
		{
			grid.EndEdit();
			if (editing == null)
			{
				return true;
			}
			string name = nameBox.Text.Trim();
			if (name.Length == 0)
			{
				Info("The name cannot be empty.");
				nameBox.Focus();
				return false;
			}

			Entry e = editing;
			List<Field> fields = new List<Field>();
			foreach (DataGridViewRow row in grid.Rows)
			{
				if (row.IsNewRow)
				{
					continue;
				}
				string fieldName = Convert.ToString(row.Cells[0].Value).Trim();
				if (fieldName.Length == 0)
				{
					continue;
				}
				Field f = new Field();
				f.Name = fieldName;
				f.Value = Convert.ToString(row.Cells[1].Value);
				f.Show = e.IsModule ? FieldShow(e, fieldName) : Math.Max(0, Array.IndexOf(ShowNames, Convert.ToString(row.Cells[2].Value)));
				fields.Add(f);
			}

			string reference = refBox.Text.Trim();
			string description = descBox.Text.Trim();
			bool connector = connectorBox.Checked;
			List<Field> all = new List<Field>();
			if (e.IsModule)
			{
				// As ConCAD's module dialog: the Name and Type fields, and the
				// library name "<Type>" with "<Name>" as description
				string type = typeBox.Text.Trim();
				Field n = new Field();
				n.Name = "Name";
				n.Value = name;
				n.Show = 2;
				all.Add(n);
				Field t = new Field();
				t.Name = "Type";
				t.Value = type;
				t.Show = FieldShow(e, "Type");
				all.Add(t);
				description = type.Length == 0 ? "" : name;
				name = type.Length == 0 ? name : type;
				connector = e.Connector;
				reference = "";
			}
			all.AddRange(fields);
			all.AddRange(hiddenFields);

			Entry updated = new Entry();
			updated.NameId = e.NameId;
			updated.SymbolId = e.SymbolId;
			updated.Type = e.Type;
			updated.Name = name;
			updated.Reference = reference;
			updated.Description = description;
			updated.Connector = connector;
			updated.Fields = all;

			LibraryPane pane = editingPane;
			try
			{
				pane.Library.Save(updated);
			}
			catch (Exception ex)
			{
				Error("The changes could not be saved.", ex);
				return false;
			}
			dirty = false;
			status.Text = "Saved \"" + updated.DisplayName + "\" in " + pane.Library.Title + "."
				+ "  The library as it was when you started is kept as " + Path.GetFileName(pane.Library.BackupPath) + ".";
			editing = null;
			Reload(pane.Library, pane, new List<long> { updated.NameId });
			Entry fresh = pane.Entries.FirstOrDefault(x => x.NameId == updated.NameId);
			ShowEntry(fresh == null ? null : pane, fresh);
			return true;
		}

		// Visibility of a module field: as stored, else shown
		// Visibility of a module field: as stored, new fields "Never shown"
		static int FieldShow(Entry e, string field)
		{
			Field f = e.Find(field);
			return f == null ? 2 : f.Show;
		}

		void Info(string text)
		{
			MessageBox.Show(this, text, AppName, MessageBoxButtons.OK, MessageBoxIcon.Information);
		}

		void Error(string text, Exception ex)
		{
			MessageBox.Show(this, text + "\n\n" + ex.Message, AppName, MessageBoxButtons.OK, MessageBoxIcon.Warning);
		}
	}

	static class Program
	{
		[STAThread]
		static void Main()
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);
			Application.Run(new MainForm());
		}
	}
}
