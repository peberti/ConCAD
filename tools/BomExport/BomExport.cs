// ConCAD BoM export helper.
//
// Fills an Excel template (.xlsx) with a bill of materials. ConCAD
// (File > Export BoM to Excel...) writes an input file and runs
//
//     BomExport.exe <input.xml>
//
// <BomExport template="...xlsx" output="...xlsx">
//   <Token name="Title" value="..."/>            design variables
//   <Item partnumber="..." type="..." name="..." qty="3"/>
// </BomExport>
//
// Exit code 0 = done. Otherwise the message is written to <input.xml>.err.
//
// Template rules:
// - {Name} in any cell (or in the page header/footer) is replaced with the
//   design variable; unknown or empty variables give an empty text. A cell
//   holding only one {Name} whose value is a number becomes a number.
// - The first row with a {BOM_...} token is the item row. It is repeated
//   once per part: {BOM_Pos}, {BOM_Partnumber}, {BOM_Type}, {BOM_Name},
//   {BOM_Qty}. Formulas in it follow their row. Rows below move down;
//   formulas, merged cells and print areas follow, and a range that ends
//   on the item row (e.g. SUM(F14:F14)) grows over all item rows.
//
// Built with the C# compiler that comes with Windows (build.bat).

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using System.Xml.Linq;

namespace ConCadBomExport
{
	class Item
	{
		public string Partnumber = "";
		public string Type = "";
		public string Name = "";
		public int Qty;
	}

	class BomException : Exception
	{
		public BomException(string message) : base(message) { }
	}

	static class Program
	{
		static readonly XNamespace S = "http://schemas.openxmlformats.org/spreadsheetml/2006/main";
		static readonly XNamespace R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
		static readonly XNamespace PR = "http://schemas.openxmlformats.org/package/2006/relationships";
		static readonly XNamespace CT = "http://schemas.openxmlformats.org/package/2006/content-types";
		static readonly XNamespace XmlNs = "http://www.w3.org/XML/1998/namespace";

		static readonly Regex TokenRx = new Regex(@"\{([^{}\r\n]+)\}");
		static readonly Regex SingleTokenRx = new Regex(@"^\s*\{([^{}\r\n]+)\}\s*$");
		// A1 style cell reference (not part of a name or a function call)
		static readonly Regex RefRx = new Regex(@"(?<![A-Za-z0-9_.])(\$?)([A-Z]{1,3})(\$?)([0-9]+)(?![0-9A-Za-z_(])");
		static readonly Regex CellRx = new Regex(@"^([A-Z]+)([0-9]+)$");

		static Dictionary<string, string> tokens = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
		static List<Item> items = new List<Item>();

		static int Main(string[] args)
		{
			if (args.Length != 1)
			{
				Console.Error.WriteLine("Usage: BomExport.exe <input.xml>  (run by ConCAD, File > Export BoM to Excel...)");
				return 2;
			}
			string errFile = args[0] + ".err";
			try
			{
				if (File.Exists(errFile))
				{
					File.Delete(errFile);
				}
				Run(args[0]);
				return 0;
			}
			catch (Exception ex)
			{
				string message = ex is BomException ? ex.Message
					: ex is IOException || ex is UnauthorizedAccessException ? ex.Message + "\r\n\r\nIs the file open in Excel?"
					: ex.ToString();
				try
				{
					File.WriteAllText(errFile, message, Encoding.UTF8);
				}
				catch
				{
				}
				Console.Error.WriteLine(message);
				return 1;
			}
		}

		static void Run(string inputFile)
		{
			XDocument input = XDocument.Load(inputFile);
			XElement root = input.Root;
			string template = (string) root.Attribute("template") ?? "";
			string output = (string) root.Attribute("output") ?? "";

			foreach (XElement t in root.Elements("Token"))
			{
				tokens[((string) t.Attribute("name") ?? "").Trim()] = (string) t.Attribute("value") ?? "";
			}
			foreach (XElement e in root.Elements("Item"))
			{
				Item it = new Item();
				it.Partnumber = (string) e.Attribute("partnumber") ?? "";
				it.Type = (string) e.Attribute("type") ?? "";
				it.Name = (string) e.Attribute("name") ?? "";
				int.TryParse((string) e.Attribute("qty") ?? "0", out it.Qty);
				items.Add(it);
			}
			if (items.Count == 0)
			{
				items.Add(new Item());
			}

			if (!File.Exists(template))
			{
				throw new BomException("The BoM template was not found:\r\n" + template);
			}
			File.Copy(template, output, true);
			File.SetAttributes(output, FileAttributes.Normal);

			using (ZipArchive zip = ZipFile.Open(output, ZipArchiveMode.Update))
			{
				Fill(zip);
			}
		}

		// ---- The workbook

		static void Fill(ZipArchive zip)
		{
			// Shared strings: keep the texts as written (the item row needs
			// its {BOM_...} tokens), substitute the variables in the others
			XDocument sst = LoadPart(zip, "xl/sharedStrings.xml");
			List<string> raw = new List<string>();
			if (sst != null)
			{
				foreach (XElement si in sst.Root.Elements(S + "si"))
				{
					string text = string.Concat(si.Descendants(S + "t").Select(t => t.Value));
					raw.Add(text);
					if (text.IndexOf("{BOM_", StringComparison.OrdinalIgnoreCase) < 0)
					{
						foreach (XElement t in si.Descendants(S + "t"))
						{
							if (t.Value.IndexOf('{') >= 0)
							{
								SetText(t, Resolve(t.Value, null, 0));
							}
						}
					}
				}
				SavePart(zip, "xl/sharedStrings.xml", sst);
			}

			XDocument workbook = LoadPart(zip, "xl/workbook.xml");
			if (workbook == null)
			{
				throw new BomException("The template is not an Excel workbook (.xlsx).");
			}
			XDocument rels = LoadPart(zip, "xl/_rels/workbook.xml.rels");

			bool found = false;
			foreach (XElement sheet in workbook.Root.Descendants(S + "sheet"))
			{
				string name = (string) sheet.Attribute("name") ?? "";
				string rid = (string) sheet.Attribute(R + "id") ?? "";
				XElement rel = rels == null ? null : rels.Root.Elements(PR + "Relationship")
					.FirstOrDefault(r => (string) r.Attribute("Id") == rid);
				if (rel == null)
				{
					continue;
				}
				string target = (string) rel.Attribute("Target") ?? "";
				string part = target.StartsWith("/") ? target.Substring(1) : "xl/" + target;
				XDocument ws = LoadPart(zip, part);
				if (ws == null)
				{
					continue;
				}
				int markerRow, shift;
				if (FillSheet(ws, raw, out markerRow, out shift))
				{
					found = true;
					ShiftDefinedNames(workbook, name, markerRow, shift);
				}
				SavePart(zip, part, ws);
			}
			if (!found)
			{
				throw new BomException("The template has no item row: put {BOM_Partnumber} (and {BOM_Qty}, ...) in the row where the parts shall be listed.");
			}

			// Let Excel recalculate everything; the old calculation chain is
			// wrong after rows were added
			XElement calcPr = workbook.Root.Element(S + "calcPr");
			if (calcPr == null)
			{
				calcPr = new XElement(S + "calcPr");
				XElement after = workbook.Root.Element(S + "definedNames") ?? workbook.Root.Element(S + "sheets");
				after.AddAfterSelf(calcPr);
			}
			calcPr.SetAttributeValue("fullCalcOnLoad", "1");
			SavePart(zip, "xl/workbook.xml", workbook);
			RemoveCalcChain(zip, rels);
		}

		// Fill one worksheet; false if it has no item row
		static bool FillSheet(XDocument ws, List<string> raw, out int markerRow, out int shift)
		{
			markerRow = 0;
			shift = 0;
			XElement sheetData = ws.Root.Element(S + "sheetData");
			if (sheetData == null)
			{
				return false;
			}

			// Variables in all cells, numbers where a cell is just one variable
			foreach (XElement c in sheetData.Descendants(S + "c"))
			{
				string text = RawText(c, raw);
				if (text == null || text.IndexOf("{BOM_", StringComparison.OrdinalIgnoreCase) >= 0)
				{
					continue;
				}
				if ((string) c.Attribute("t") == "inlineStr" || SingleTokenRx.IsMatch(text))
				{
					SetCellValue(c, Resolve(text, null, 0), SingleTokenRx.IsMatch(text));
				}
			}
			// ... and in the page header/footer
			XElement hf = ws.Root.Element(S + "headerFooter");
			if (hf != null)
			{
				foreach (XElement e in hf.Elements())
				{
					e.Value = Resolve(e.Value, null, 0);
				}
			}

			XElement marker = sheetData.Elements(S + "row").FirstOrDefault(row =>
				row.Elements(S + "c").Any(c =>
				{
					string t = RawText(c, raw);
					return t != null && t.IndexOf("{BOM_", StringComparison.OrdinalIgnoreCase) >= 0;
				}));
			if (marker == null)
			{
				return false;
			}
			markerRow = (int) marker.Attribute("r");
			shift = items.Count - 1;
			int m = markerRow, sh = shift;

			// Rows below move down
			foreach (XElement row in sheetData.Elements(S + "row"))
			{
				int r = (int) row.Attribute("r");
				if (r > m)
				{
					row.SetAttributeValue("r", r + sh);
					foreach (XElement c in row.Elements(S + "c"))
					{
						MoveCell(c, r + sh);
					}
				}
			}
			// Formulas everywhere follow
			foreach (XElement f in sheetData.Descendants(S + "f"))
			{
				if (f.Parent.Parent != marker)
				{
					f.Value = ShiftRefs(f.Value, m, sh);
					if (f.Attribute("ref") != null)
					{
						f.SetAttributeValue("ref", ShiftRefs((string) f.Attribute("ref"), m, sh));
					}
				}
			}

			// The item rows
			List<XElement> clones = new List<XElement>();
			for (int i = 0; i < items.Count; i++)
			{
				XElement row = new XElement(marker);
				row.SetAttributeValue("r", m + i);
				foreach (XElement c in row.Elements(S + "c"))
				{
					string text = RawText(c, raw);
					MoveCell(c, m + i);
					if (text != null && text.IndexOf('{') >= 0)
					{
						SetCellValue(c, Resolve(text, items[i], i + 1), SingleTokenRx.IsMatch(text));
					}
					XElement f = c.Element(S + "f");
					if (f != null)
					{
						if ((string) f.Attribute("t") == "shared")
						{
							f.SetAttributeValue("t", null);
							f.SetAttributeValue("ref", null);
							f.SetAttributeValue("si", null);
						}
						if (f.Value.Length == 0)
						{
							f.Remove();
						}
						else
						{
							f.Value = CloneRefs(f.Value, m, i, sh);
							XElement v = c.Element(S + "v");
							if (v != null)
							{
								v.Remove();
							}
						}
					}
				}
				clones.Add(row);
			}
			marker.ReplaceWith(clones);

			// Merged cells, formatting ranges, links, the used range
			XElement merges = ws.Root.Element(S + "mergeCells");
			if (merges != null)
			{
				foreach (XElement mc in merges.Elements(S + "mergeCell").ToList())
				{
					string rf = (string) mc.Attribute("ref") ?? "";
					int r1, r2;
					if (RangeRows(rf, out r1, out r2) && r1 == m && r2 == m)
					{
						for (int i = 1; i < items.Count; i++)
						{
							mc.AddAfterSelf(new XElement(S + "mergeCell", new XAttribute("ref", CloneRefs(rf, m, i, sh))));
						}
					}
					else
					{
						mc.SetAttributeValue("ref", ShiftRefs(rf, m, sh));
					}
				}
				merges.SetAttributeValue("count", merges.Elements(S + "mergeCell").Count());
			}
			foreach (XElement e in ws.Root.Descendants())
			{
				foreach (string a in new[] { "sqref", "ref" })
				{
					XAttribute at = e.Attribute(a);
					if (at != null && e.Name != S + "mergeCell" && e.Name != S + "f")
					{
						at.Value = ShiftRefs(at.Value, m, sh);
					}
				}
			}
			return true;
		}

		// ---- Cells

		// The text of a string cell as written in the template (null if the
		// cell is not a text)
		static string RawText(XElement c, List<string> raw)
		{
			string t = (string) c.Attribute("t");
			if (t == "s")
			{
				int i;
				XElement v = c.Element(S + "v");
				if (v != null && int.TryParse(v.Value, out i) && i >= 0 && i < raw.Count)
				{
					return raw[i];
				}
				return null;
			}
			if (t == "inlineStr")
			{
				XElement isEl = c.Element(S + "is");
				return isEl == null ? "" : string.Concat(isEl.Descendants(S + "t").Select(x => x.Value));
			}
			if (t == "str" && c.Element(S + "f") == null)
			{
				XElement v = c.Element(S + "v");
				return v == null ? "" : v.Value;
			}
			return null;
		}

		// Write a text, or a number if asked and the text is one
		static void SetCellValue(XElement c, string value, bool numberIfPossible)
		{
			c.Elements(S + "v").Remove();
			c.Elements(S + "is").Remove();
			double d;
			if (numberIfPossible && value.Trim().Length > 0 && TryNumber(value.Trim(), out d))
			{
				c.SetAttributeValue("t", null);
				c.Add(new XElement(S + "v", d.ToString("R", CultureInfo.InvariantCulture)));
			}
			else
			{
				c.SetAttributeValue("t", "inlineStr");
				XElement t = new XElement(S + "t");
				SetText(t, value);
				c.Add(new XElement(S + "is", t));
			}
		}

		static bool TryNumber(string s, out double d)
		{
			return double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out d)
				|| double.TryParse(s, NumberStyles.Float, CultureInfo.CurrentCulture, out d);
		}

		static void SetText(XElement t, string value)
		{
			t.Value = value;
			if (value.Length > 0 && (char.IsWhiteSpace(value[0]) || char.IsWhiteSpace(value[value.Length - 1]) || value.Contains("\n")))
			{
				t.SetAttributeValue(XmlNs + "space", "preserve");
			}
		}

		static void MoveCell(XElement c, int row)
		{
			Match mm = CellRx.Match((string) c.Attribute("r") ?? "");
			if (mm.Success)
			{
				c.SetAttributeValue("r", mm.Groups[1].Value + row);
			}
		}

		// ---- Variables

		static string Resolve(string s, Item item, int pos)
		{
			return TokenRx.Replace(s, delegate(Match mm)
			{
				string name = mm.Groups[1].Value.Trim();
				if (item != null)
				{
					switch (name.ToUpperInvariant())
					{
						case "BOM_POS": return pos.ToString(CultureInfo.InvariantCulture);
						case "BOM_PARTNUMBER": return item.Partnumber;
						case "BOM_TYPE": return item.Type;
						case "BOM_NAME": return item.Name;
						case "BOM_QTY": return item.Qty > 0 ? item.Qty.ToString(CultureInfo.InvariantCulture) : "";
					}
				}
				string v;
				return tokens.TryGetValue(name, out v) ? v : "";
			});
		}

		// ---- References

		// Rows below the item row move by 'shift'; a range that ends on the
		// item row grows over all item rows
		static string ShiftRefs(string s, int markerRow, int shift)
		{
			if (shift == 0 || string.IsNullOrEmpty(s))
			{
				return s;
			}
			return RefRx.Replace(s, delegate(Match mm)
			{
				int row = int.Parse(mm.Groups[4].Value, CultureInfo.InvariantCulture);
				bool rangeEnd = mm.Index > 0 && s[mm.Index - 1] == ':';
				if (row > markerRow || (row == markerRow && rangeEnd))
				{
					row += shift;
				}
				return mm.Groups[1].Value + mm.Groups[2].Value + mm.Groups[3].Value + row;
			});
		}

		// A formula of the item row copied to item row 'index': references
		// to its own row (not $-fixed) follow
		static string CloneRefs(string s, int markerRow, int index, int shift)
		{
			return RefRx.Replace(s, delegate(Match mm)
			{
				int row = int.Parse(mm.Groups[4].Value, CultureInfo.InvariantCulture);
				if (row == markerRow && mm.Groups[3].Value.Length == 0)
				{
					row += index;
				}
				else if (row > markerRow)
				{
					row += shift;
				}
				return mm.Groups[1].Value + mm.Groups[2].Value + mm.Groups[3].Value + row;
			});
		}

		static bool RangeRows(string rf, out int r1, out int r2)
		{
			r1 = r2 = 0;
			MatchCollection mc = RefRx.Matches(rf);
			if (mc.Count == 0)
			{
				return false;
			}
			r1 = int.Parse(mc[0].Groups[4].Value, CultureInfo.InvariantCulture);
			r2 = int.Parse(mc[mc.Count - 1].Groups[4].Value, CultureInfo.InvariantCulture);
			return true;
		}

		// Print area / titles etc. of this sheet
		static void ShiftDefinedNames(XDocument workbook, string sheetName, int markerRow, int shift)
		{
			XElement names = workbook.Root.Element(S + "definedNames");
			if (names == null)
			{
				return;
			}
			foreach (XElement dn in names.Elements(S + "definedName"))
			{
				string v = dn.Value;
				if (v.Contains(sheetName + "!") || v.Contains("'" + sheetName.Replace("'", "''") + "'!"))
				{
					dn.Value = ShiftRefs(v, markerRow, shift);
				}
			}
		}

		// ---- Package parts

		static XDocument LoadPart(ZipArchive zip, string name)
		{
			ZipArchiveEntry e = zip.GetEntry(name);
			if (e == null)
			{
				return null;
			}
			using (Stream s = e.Open())
			{
				return XDocument.Load(s, LoadOptions.PreserveWhitespace);
			}
		}

		static void SavePart(ZipArchive zip, string name, XDocument doc)
		{
			ZipArchiveEntry old = zip.GetEntry(name);
			if (old != null)
			{
				old.Delete();
			}
			ZipArchiveEntry e = zip.CreateEntry(name, CompressionLevel.Optimal);
			XmlWriterSettings ws = new XmlWriterSettings();
			ws.Encoding = new UTF8Encoding(false);
			using (Stream s = e.Open())
			using (XmlWriter w = XmlWriter.Create(s, ws))
			{
				doc.Save(w);
			}
		}

		static void RemoveCalcChain(ZipArchive zip, XDocument rels)
		{
			ZipArchiveEntry chain = zip.GetEntry("xl/calcChain.xml");
			if (chain == null)
			{
				return;
			}
			chain.Delete();
			if (rels != null)
			{
				rels.Root.Elements(PR + "Relationship")
					.Where(r => ((string) r.Attribute("Target") ?? "").EndsWith("calcChain.xml")).Remove();
				SavePart(zip, "xl/_rels/workbook.xml.rels", rels);
			}
			XDocument types = LoadPart(zip, "[Content_Types].xml");
			if (types != null)
			{
				types.Root.Elements(CT + "Override")
					.Where(o => ((string) o.Attribute("PartName") ?? "").EndsWith("calcChain.xml")).Remove();
				SavePart(zip, "[Content_Types].xml", types);
			}
		}
	}
}
