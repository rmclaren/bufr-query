# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'bufr-query'
copyright = '2024 NOAA/NWS/NCEP/EMC'
author = ''

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = ['sphinx.ext.mathjax',
              'sphinx.ext.todo',
              'sphinx.ext.autodoc',
              'sphinx.ext.autosectionlabel',
              'sphinxcontrib.plantuml']

templates_path = ['_templates']
exclude_patterns = ['.DS_Store']

root_doc = 'index'

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'default'
html_static_path = ['_static']
html_sidebars = {'**': ['globaltoc.html', 'relations.html', 'sourcelink.html', 'searchbox.html']}
